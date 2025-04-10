#include "render_graph_pass.h"
#include "render_graph.h"
#include "renderer.h"
#include "EASTL/algorithm.h"

RenderGraphPassBase::RenderGraphPassBase(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph) :
    DAGNode(graph)
{
    m_name = name;
    m_type = type;
}

//todo : https://docs.microsoft.com/en-us/windows/win32/direct3d12/executing-and-synchronizing-command-lists#accessing-resources-from-multiple-command-queues
void RenderGraphPassBase::ResolveBarriers(const DirectedAcyclicGraph& graph)
{
    eastl::vector<DAGEdge*> edges;

    eastl::vector<DAGEdge*> resource_incoming;
    eastl::vector<DAGEdge*> resource_outgoing;

    graph.GetIncomingEdges(this, edges);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
        RE_ASSERT(edge->GetToNode() == this->GetId());

        RenderGraphResourceNode* resource_node = (RenderGraphResourceNode*)graph.GetNode(edge->GetFromNode());
        RenderGraphResource* resource = resource_node->GetResource();

        graph.GetIncomingEdges(resource_node, resource_incoming);
        graph.GetOutgoingEdges(resource_node, resource_outgoing);
        RE_ASSERT(resource_incoming.size() <= 1);
        RE_ASSERT(resource_outgoing.size() >= 1);

        GfxAccessFlags old_state = GfxAccessPresent;
        GfxAccessFlags new_state = edge->GetUsage();

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
        if (old_state == GfxAccessPresent)
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

        bool is_aliased = false;
        GfxAccessFlags alias_state;

        if (resource->IsOverlapping() && resource->GetFirstPassID() == this->GetId())
        {
            IGfxResource* aliased_resource = resource->GetAliasedPrevResource(alias_state);
            if (aliased_resource)
            {
                m_discardBarriers.push_back({ aliased_resource, alias_state, new_state | GfxAccessDiscard });

                is_aliased = true;
            }
        }

        if (old_state != new_state || is_aliased)
        {
            //TODO : uav barrier
            ResourceBarrier barrier;
            barrier.resource = resource;
            barrier.sub_resource = edge->GetSubresource();
            barrier.old_state = old_state;
            barrier.new_state = new_state;

            if (is_aliased)
            {
                barrier.old_state |= alias_state | GfxAccessDiscard;
            }

            m_resourceBarriers.push_back(barrier);
        }
    }

    graph.GetOutgoingEdges(this, edges);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
        RE_ASSERT(edge->GetFromNode() == this->GetId());

        GfxAccessFlags new_state = edge->GetUsage();

        if (new_state == GfxAccessRTV)
        {
            RE_ASSERT(dynamic_cast<RenderGraphEdgeColorAttchment*>(edge) != nullptr);

            RenderGraphEdgeColorAttchment* color_rt = (RenderGraphEdgeColorAttchment*)edge;
            m_pColorRT[color_rt->GetColorIndex()] = color_rt;
        }
        else if (new_state == GfxAccessDSV || new_state == GfxAccessDSVReadOnly)
        {
            RE_ASSERT(dynamic_cast<RenderGraphEdgeDepthAttchment*>(edge) != nullptr);

            m_pDepthRT = (RenderGraphEdgeDepthAttchment*)edge;
        }
    }
}

void RenderGraphPassBase::ResolveAsyncCompute(const DirectedAcyclicGraph& graph, RenderGraphAsyncResolveContext& context)
{
    if (m_type == RenderPassType::AsyncCompute)
    {
        eastl::vector<DAGEdge*> edges;

        eastl::vector<DAGEdge*> resource_incoming;
        eastl::vector<DAGEdge*> resource_outgoing;

        graph.GetIncomingEdges(this, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
            RE_ASSERT(edge->GetToNode() == this->GetId());

            RenderGraphResourceNode* resource_node = (RenderGraphResourceNode*)graph.GetNode(edge->GetFromNode());

            graph.GetIncomingEdges(resource_node, resource_incoming);
            RE_ASSERT(resource_incoming.size() <= 1);

            if(!resource_incoming.empty())
            {
                RenderGraphPassBase* prePass = (RenderGraphPassBase*)graph.GetNode(resource_incoming[0]->GetFromNode());
                if (!prePass->IsCulled() && prePass->GetType() != RenderPassType::AsyncCompute)
                {
                    context.preGraphicsQueuePasses.push_back(prePass->GetId());
                }
            }
        }

        graph.GetOutgoingEdges(this, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
            RE_ASSERT(edge->GetFromNode() == this->GetId());

            RenderGraphResourceNode* resource_node = (RenderGraphResourceNode*)graph.GetNode(edge->GetToNode());
            graph.GetOutgoingEdges(resource_node, resource_outgoing);

            for (size_t i = 0; i < resource_outgoing.size(); i++)
            {
                RenderGraphPassBase* postPass = (RenderGraphPassBase*)graph.GetNode(resource_outgoing[i]->GetToNode());
                if (!postPass->IsCulled() && postPass->GetType() != RenderPassType::AsyncCompute)
                {
                    context.postGraphicsQueuePasses.push_back(postPass->GetId());
                }
            }
        }

        context.computeQueuePasses.push_back(GetId());
    }
    else
    {
        if (!context.computeQueuePasses.empty())
        {
            if (!context.preGraphicsQueuePasses.empty())
            {
                DAGNodeID graphicsPassToWaitID = *eastl::max_element(context.preGraphicsQueuePasses.begin(), context.preGraphicsQueuePasses.end());

                RenderGraphPassBase* graphicsPassToWait = (RenderGraphPassBase*)graph.GetNode(graphicsPassToWaitID);
                if (graphicsPassToWait->m_signalValue == -1)
                {
                    graphicsPassToWait->m_signalValue = ++context.graphicsFence;
                }

                RenderGraphPassBase* computePass = (RenderGraphPassBase*)graph.GetNode(context.computeQueuePasses[0]);
                computePass->m_waitValue = graphicsPassToWait->m_signalValue;

                for (size_t i = 0; i < context.computeQueuePasses.size(); ++i)
                {
                    RenderGraphPassBase* computePass = (RenderGraphPassBase*)graph.GetNode(context.computeQueuePasses[i]);
                    computePass->m_waitGraphicsPass = graphicsPassToWaitID;
                }
            }

            if (!context.postGraphicsQueuePasses.empty())
            {
                DAGNodeID graphicsPassToSignalID = *eastl::min_element(context.postGraphicsQueuePasses.begin(), context.postGraphicsQueuePasses.end());

                RenderGraphPassBase* computePass = (RenderGraphPassBase*)graph.GetNode(context.computeQueuePasses.back());
                if (computePass->m_signalValue == -1)
                {
                    computePass->m_signalValue = ++context.computeFence;
                }

                RenderGraphPassBase* graphicsPassToSignal = (RenderGraphPassBase*)graph.GetNode(graphicsPassToSignalID);
                graphicsPassToSignal->m_waitValue = computePass->m_signalValue;

                for (size_t i = 0; i < context.computeQueuePasses.size(); ++i)
                {
                    RenderGraphPassBase* computePass = (RenderGraphPassBase*)graph.GetNode(context.computeQueuePasses[i]);
                    computePass->m_signalGraphicsPass = graphicsPassToSignalID;
                }
            }            

            context.computeQueuePasses.clear();
            context.preGraphicsQueuePasses.clear();
            context.postGraphicsQueuePasses.clear();
        }
    }
}

void RenderGraphPassBase::Execute(const RenderGraph& graph, RenderGraphPassExecuteContext& context)
{
    IGfxCommandList* pCommandList = m_type == RenderPassType::AsyncCompute ? context.computeCommandList : context.graphicsCommandList;

    if (m_waitValue != -1)
    {
        pCommandList->End();
        pCommandList->Submit();

        pCommandList->Begin();
        context.renderer->SetupGlobalConstants(pCommandList);

        if (m_type == RenderPassType::AsyncCompute)
        {
            pCommandList->Wait(context.graphicsQueueFence, context.initialGraphicsFenceValue + m_waitValue);
        }
        else
        {
            pCommandList->Wait(context.computeQueueFence, context.initialComputeFenceValue + m_waitValue);
        }
    }

    for (size_t i = 0; i < m_eventNames.size(); ++i)
    {
        context.graphicsCommandList->BeginEvent(m_eventNames[i]);
    }

    if (!IsCulled())
    {        
        GPU_EVENT(pCommandList, m_name);

        Begin(graph, pCommandList);
        ExecuteImpl(pCommandList);
        End(pCommandList);
    }

    for (uint32_t i = 0; i < m_nEndEventNum; ++i)
    {
        context.graphicsCommandList->EndEvent();
    }

    if (m_signalValue != -1)
    {
        pCommandList->End();
        if (m_type == RenderPassType::AsyncCompute)
        {
            pCommandList->Signal(context.computeQueueFence, context.initialComputeFenceValue + m_signalValue);
            context.lastSignaledComputeValue = context.initialComputeFenceValue + m_signalValue;
        }
        else
        {
            pCommandList->Signal(context.graphicsQueueFence, context.initialGraphicsFenceValue + m_signalValue);
            context.lastSignaledGraphicsValue = context.initialGraphicsFenceValue + m_signalValue;
        }
        pCommandList->Submit();

        pCommandList->Begin();
        context.renderer->SetupGlobalConstants(pCommandList);
    }
}

void RenderGraphPassBase::Begin(const RenderGraph& graph, IGfxCommandList* pCommandList)
{
    for (size_t i = 0; i < m_discardBarriers.size(); ++i)
    {
        const AliasDiscardBarrier& barrier = m_discardBarriers[i];

        if (barrier.resource->IsTexture())
        {
            pCommandList->TextureBarrier((IGfxTexture*)barrier.resource, GFX_ALL_SUB_RESOURCE, barrier.acess_before, barrier.acess_after);
        }
        else
        {
            pCommandList->BufferBarrier((IGfxBuffer*)barrier.resource, barrier.acess_before, barrier.acess_after);
        }
    }

    for (size_t i = 0; i < m_resourceBarriers.size(); ++i)
    {
        const ResourceBarrier& barrier = m_resourceBarriers[i];
        barrier.resource->Barrier(pCommandList, barrier.sub_resource, barrier.old_state, barrier.new_state);
    }

    if (HasGfxRenderPass())
    {
        GfxRenderPassDesc desc;

        for (int i = 0; i < 8; ++i)
        {
            if (m_pColorRT[i] != nullptr)
            {
                RenderGraphResourceNode* node = (RenderGraphResourceNode*)graph.GetDAG().GetNode(m_pColorRT[i]->GetToNode());
                IGfxTexture* texture = ((RGTexture*)node->GetResource())->GetTexture();

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
            IGfxTexture* texture = ((RGTexture*)node->GetResource())->GetTexture();

            uint32_t mip, slice;
            DecomposeSubresource(texture->GetDesc(), m_pDepthRT->GetSubresource(), mip, slice);

            desc.depth.texture = ((RGTexture*)node->GetResource())->GetTexture();
            desc.depth.load_op = m_pDepthRT->GetDepthLoadOp();
            desc.depth.mip_slice = mip;
            desc.depth.array_slice = slice;
            desc.depth.store_op = node->IsCulled() ? GfxRenderPassStoreOp::DontCare : GfxRenderPassStoreOp::Store;
            desc.depth.stencil_load_op = m_pDepthRT->GetStencilLoadOp();
            desc.depth.stencil_store_op = node->IsCulled() ? GfxRenderPassStoreOp::DontCare : GfxRenderPassStoreOp::Store;
            desc.depth.clear_depth = m_pDepthRT->GetClearDepth();
            desc.depth.clear_stencil = m_pDepthRT->GetClearStencil();
            desc.depth.read_only = m_pDepthRT->IsReadOnly();
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