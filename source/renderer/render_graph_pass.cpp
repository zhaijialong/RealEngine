#include "render_graph_pass.h"
#include "render_graph.h"

RenderGraphPassBase::RenderGraphPassBase(const std::string& name, DirectedAcyclicGraph& graph) :
    DAGNode(graph)
{
    m_name = name;
}

void RenderGraphPassBase::Resolve(const DirectedAcyclicGraph& graph)
{
    std::vector<DAGEdge*> incoming_edge = graph.GetIncomingEdges(this);

    for (size_t i = 0; i < incoming_edge.size(); ++i)
    {
        RenderGraphEdge* edge = (RenderGraphEdge*)incoming_edge[i];
        RE_ASSERT(edge->GetToNode() == this->GetId());

        RenderGraphResourceNode* resource_node = (RenderGraphResourceNode*)graph.GetNode(edge->GetFromNode());
        RenderGraphResource* resource = resource_node->GetResource();

        std::vector<DAGEdge*> resource_incoming = graph.GetIncomingEdges(resource_node);
        std::vector<DAGEdge*> resource_outgoing = graph.GetOutgoingEdges(resource_node);
        RE_ASSERT(resource_incoming.size() <= 1);
        RE_ASSERT(resource_outgoing.size() >= 1);

        GfxResourceState old_state = GfxResourceState::Present;
        GfxResourceState new_state = edge->GetUsage();

        //try to find previous state from last pass which used this resource
        if (resource_outgoing.size() > 1) //todo : should merge states if possible, eg. shader resource ps + shader resource non-ps -> shader resource all
        {
            //resource_outgoing should be sorted
            for (int i = (int)resource_outgoing.size() - 1; i >= 0; --i)
            {
                uint32_t subresource = ((RenderGraphEdge*)resource_outgoing[i])->GetSubresource();
                DAGNodeID pass_id = resource_outgoing[i]->GetToNode();
                if (subresource == edge->GetSubresource() && pass_id < this->GetId() && !graph.GetNode(pass_id)->IsCulled())
                {
                    old_state = ((RenderGraphEdge*)resource_outgoing[i])->GetUsage();
                    break;
                }
            }
        }

        //if not found, get the state from the pass which output the resource
        if (old_state == GfxResourceState::Present)
        {
            if (resource_incoming.empty())
            {
                RE_ASSERT(resource_node->GetVersion() == 0);
                old_state = resource->GetInitialState();
            }
            else
            {
                old_state = ((RenderGraphEdge*)resource_incoming[0])->GetUsage();
            }
        }

        if (old_state != new_state)
        {
            //TODO : uav barrier
            ResourceBarrier barrier;
            barrier.resource = resource;
            barrier.sub_resource = edge->GetSubresource();
            barrier.old_state = old_state;
            barrier.new_state = new_state;

            m_resourceBarriers.push_back(barrier);
        }

        if (resource->IsOverlapping() && resource->GetFirstPassID() == this->GetId())
        {
            IGfxResource* aliased_resource = resource->GetAliasedPrevResource();
            if (aliased_resource)
            {
                AliasBarrier barrier = { aliased_resource, resource->GetResource() };

                m_aliasBarriers.push_back(barrier);
            }
        }
    }

    std::vector<DAGEdge*> outgoing_edge = graph.GetOutgoingEdges(this);

    for (size_t i = 0; i < outgoing_edge.size(); ++i)
    {
        RenderGraphEdge* edge = (RenderGraphEdge*)outgoing_edge[i];
        RE_ASSERT(edge->GetFromNode() == this->GetId());

        GfxResourceState new_state = edge->GetUsage();

        if (new_state == GfxResourceState::RenderTarget)
        {
            RE_ASSERT(dynamic_cast<RenderGraphEdgeColorAttchment*>(edge) != nullptr);

            RenderGraphEdgeColorAttchment* color_rt = (RenderGraphEdgeColorAttchment*)edge;
            m_pColorRT[color_rt->GetColorIndex()] = color_rt;
        }
        else if (new_state == GfxResourceState::DepthStencil || new_state == GfxResourceState::DepthStencilReadOnly)
        {
            RE_ASSERT(dynamic_cast<RenderGraphEdgeDepthAttchment*>(edge) != nullptr);

            m_pDepthRT = (RenderGraphEdgeDepthAttchment*)edge;
        }
    }
}

void RenderGraphPassBase::Execute(const RenderGraph& graph, IGfxCommandList* pCommandList)
{
    for (size_t i = 0; i < m_eventNames.size(); ++i)
    {
        pCommandList->BeginEvent(m_eventNames[i]);
    }

    {
        GPU_EVENT(pCommandList, m_name);

        Begin(graph, pCommandList);
        ExecuteImpl(pCommandList);
        End(pCommandList);
    }

    for (uint32_t i = 0; i < m_nEndEventNum; ++i)
    {
        pCommandList->EndEvent();
    }
}

void RenderGraphPassBase::Begin(const RenderGraph& graph, IGfxCommandList* pCommandList)
{
    for (size_t i = 0; i < m_aliasBarriers.size(); ++i)
    {
        pCommandList->AliasingBarrier(m_aliasBarriers[i].before, m_aliasBarriers[i].after);
    }

    for (size_t i = 0; i < m_resourceBarriers.size(); ++i)
    {
        const ResourceBarrier& barrier = m_resourceBarriers[i];

        pCommandList->ResourceBarrier(barrier.resource->GetResource(), barrier.sub_resource, barrier.old_state, barrier.new_state);
    }

    if (HasGfxRenderPass())
    {
        GfxRenderPassDesc desc;

        for (int i = 0; i < 8; ++i)
        {
            if (m_pColorRT[i] != nullptr)
            {
                RenderGraphResourceNode* node = (RenderGraphResourceNode*)graph.GetDAG().GetNode(m_pColorRT[i]->GetToNode());
                IGfxTexture* texture = ((RenderGraphTexture*)node->GetResource())->GetTexture();

                uint32_t mip, slice;
                DecomposeSubresource(texture->GetDesc(), m_pColorRT[i]->GetSubresource(), mip, slice);

                desc.color[i].texture = texture;
                desc.color[i].mip_slice = mip;
                desc.color[i].array_slice = slice;
                desc.color[i].load_op = m_pColorRT[i]->GetLoadOp();
                desc.color[i].store_op = node->IsCulled() ? GfxRenderPassStoreOp::DontCare : GfxRenderPassStoreOp::Store;
                memcpy(desc.color[i].clear_color, m_pColorRT[i]->GetClearColor(), sizeof(float) * 4);
            }
        }

        if (m_pDepthRT != nullptr)
        {
            RenderGraphResourceNode* node = (RenderGraphResourceNode*)graph.GetDAG().GetNode(m_pDepthRT->GetToNode());
            IGfxTexture* texture = ((RenderGraphTexture*)node->GetResource())->GetTexture();

            uint32_t mip, slice;
            DecomposeSubresource(texture->GetDesc(), m_pDepthRT->GetSubresource(), mip, slice);

            desc.depth.texture = ((RenderGraphTexture*)node->GetResource())->GetTexture();
            desc.depth.load_op = m_pDepthRT->GetDepthLoadOp();
            desc.depth.mip_slice = mip;
            desc.depth.array_slice = slice;
            desc.depth.store_op = node->IsCulled() ? GfxRenderPassStoreOp::DontCare : GfxRenderPassStoreOp::Store;
            desc.depth.stencil_load_op = m_pDepthRT->GetStencilLoadOp();
            desc.depth.stencil_store_op = node->IsCulled() ? GfxRenderPassStoreOp::DontCare : GfxRenderPassStoreOp::Store;
            desc.depth.clear_depth = m_pDepthRT->GetClearDepth();
            desc.depth.clear_stencil = m_pDepthRT->GetClearStencil();
        }

        pCommandList->BeginRenderPass(desc);
    }
}

void RenderGraphPassBase::End(IGfxCommandList* pCommandList)
{
    if (HasGfxRenderPass())
    {
        pCommandList->EndRenderPass();
    }
}

bool RenderGraphPassBase::HasGfxRenderPass() const
{
    for (int i = 0; i < 8; i++)
    {
        if (m_pColorRT[i] != nullptr)
        {
            return true;
        }
    }

    return m_pDepthRT != nullptr;
}