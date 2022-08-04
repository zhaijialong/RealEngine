#include "render_graph.h"
#include "core/engine.h"
#include "utils/profiler.h"

RenderGraph::RenderGraph(Renderer* pRenderer) :
    m_resourceAllocator(pRenderer->GetDevice())
{
    IGfxDevice* device = pRenderer->GetDevice();
    m_pComputeQueueFence.reset(device->CreateFence("RenderGraph::m_pComputeQueueFence"));
    m_pGraphicsQueueFence.reset(device->CreateFence("RenderGraph::m_pGraphicsQueueFence"));
}

void RenderGraph::BeginEvent(const eastl::string& name)
{
    m_eventNames.push_back(name);
}

void RenderGraph::EndEvent()
{
    if (!m_eventNames.empty())
    {
        m_eventNames.pop_back();
    }
    else
    {
        m_passes.back()->EndEvent();
    }
}

void RenderGraph::Clear()
{
    for (size_t i = 0; i < m_objFinalizer.size(); ++i)
    {
        m_objFinalizer[i].finalizer(m_objFinalizer[i].obj);
    }
    m_objFinalizer.clear();

    m_graph.Clear();

    m_passes.clear();
    m_resourceNodes.clear();
    m_resources.clear();

    m_allocator.Reset();
    m_resourceAllocator.Reset();

    m_outputResources.clear();
}

void RenderGraph::Compile()
{
    CPU_EVENT("Render", "RenderGraph::Compile");

    m_graph.Cull();

    RenderGraphAsyncResolveContext context;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pass = m_passes[i];
        if (!pass->IsCulled())
        {
            pass->ResolveAsyncCompute(m_graph, context);
        }
    }

    eastl::vector<DAGEdge*> edges;

    for (size_t i = 0; i < m_resourceNodes.size(); ++i)
    {
        RenderGraphResourceNode* node = m_resourceNodes[i];
        if (node->IsCulled())
        {
            continue;
        }

        RenderGraphResource* resource = node->GetResource();

        m_graph.GetOutgoingEdges(node, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
            RenderGraphPassBase* pass = (RenderGraphPassBase*)m_graph.GetNode(edge->GetToNode());

            if (!pass->IsCulled())
            {
                resource->Resolve(edge, pass);
            }
        }

        m_graph.GetIncomingEdges(node, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* edge = (RenderGraphEdge*)edges[i];
            RenderGraphPassBase* pass = (RenderGraphPassBase*)m_graph.GetNode(edge->GetToNode());

            if (!pass->IsCulled())
            {
                resource->Resolve(edge, pass);
            }
        }
    }

    for (size_t i = 0; i < m_resources.size(); ++i)
    {
        RenderGraphResource* resource = m_resources[i];
        if (resource->IsUsed())
        {
            resource->Realize();
        }
    }

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pass = m_passes[i];
        if (!pass->IsCulled())
        {
            pass->ResolveBarriers(m_graph);
        }
    }
}

void RenderGraph::Execute(Renderer* pRenderer, IGfxCommandList* pCommandList, IGfxCommandList* pComputeCommandList)
{
    CPU_EVENT("Render", "RenderGraph::Execute");
    GPU_EVENT(pCommandList, "RenderGraph");

    RenderGraphPassExecuteContext context = {};
    context.renderer = pRenderer;
    context.graphicsCommandList = pCommandList;
    context.computeCommandList = pComputeCommandList;
    context.computeQueueFence = m_pComputeQueueFence.get();
    context.graphicsQueueFence = m_pGraphicsQueueFence.get();
    context.initialComputeFenceValue = m_nComputeQueueFenceValue;
    context.initialGraphicsFenceValue = m_nGraphicsQueueFenceValue;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pass = m_passes[i];

        pass->Execute(*this, context);
    }

    m_nComputeQueueFenceValue = context.lastSignaledComputeValue;
    m_nGraphicsQueueFenceValue = context.lastSignaledGraphicsValue;

    for (size_t i = 0; i < m_outputResources.size(); ++i)
    {
        const PresentTarget& target = m_outputResources[i];
        if (target.resource->GetFinalState() != target.state)
        {
            pCommandList->ResourceBarrier(target.resource->GetResource(), 0, target.resource->GetFinalState(), target.state);
            target.resource->SetFinalState(target.state);
        }
    }
    m_outputResources.clear();
}

void RenderGraph::Present(const RenderGraphHandle& handle, GfxResourceState filnal_state)
{
    RE_ASSERT(handle.IsValid());

    RenderGraphResource* resource = GetTexture(handle);
    resource->SetOutput(true);

    RenderGraphResourceNode* node = m_resourceNodes[handle.node];
    node->MakeTarget();

    PresentTarget target;
    target.resource = resource;
    target.state = filnal_state;
    m_outputResources.push_back(target);
}

RenderGraphTexture* RenderGraph::GetTexture(const RenderGraphHandle& handle)
{
    RE_ASSERT(handle.IsValid());
    RenderGraphResource* resource = m_resources[handle.index];
    RE_ASSERT(dynamic_cast<RenderGraphTexture*>(resource) != nullptr);
    return (RenderGraphTexture*)resource;
}

RenderGraphBuffer* RenderGraph::GetBuffer(const RenderGraphHandle& handle)
{
    RE_ASSERT(handle.IsValid());
    RenderGraphResource* resource = m_resources[handle.index];
    RE_ASSERT(dynamic_cast<RenderGraphBuffer*>(resource) != nullptr);
    return (RenderGraphBuffer*)resource;
}

bool RenderGraph::Export(const eastl::string& file)
{
    return m_graph.ExportGraphviz(file.c_str());
}

RenderGraphHandle RenderGraph::Import(IGfxTexture* texture, GfxResourceState state)
{
    auto resource = Allocate<RenderGraphTexture>(m_resourceAllocator, texture, state);
    auto node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RenderGraphHandle handle;
    handle.index = (uint16_t)m_resources.size();
    handle.node = (uint16_t)m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}

RenderGraphHandle RenderGraph::Import(IGfxBuffer* buffer, GfxResourceState state)
{
    auto resource = Allocate<RenderGraphBuffer>(m_resourceAllocator, buffer, state);
    auto node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RenderGraphHandle handle;
    handle.index = (uint16_t)m_resources.size();
    handle.node = (uint16_t)m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}

RenderGraphHandle RenderGraph::Read(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RE_ASSERT(input.IsValid());
    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];

    AllocatePOD<RenderGraphEdge>(m_graph, input_node, pass, usage, subresource);

    return input;
}

RenderGraphHandle RenderGraph::Write(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RE_ASSERT(input.IsValid());
    RenderGraphResource* resource = m_resources[input.index];

    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];
    AllocatePOD<RenderGraphEdge>(m_graph, input_node, pass, usage, subresource);

    RenderGraphResourceNode* output_node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, input_node->GetVersion() + 1);
    AllocatePOD<RenderGraphEdge>(m_graph, pass, output_node, usage, subresource);

    RenderGraphHandle output;        
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}

RenderGraphHandle RenderGraph::WriteColor(RenderGraphPassBase* pass, uint32_t color_index, const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp load_op, const float4& clear_color)
{
    RE_ASSERT(input.IsValid());
    RenderGraphResource* resource = m_resources[input.index];

    GfxResourceState usage = GfxResourceState::RenderTarget;

    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];
    AllocatePOD<RenderGraphEdgeColorAttchment>(m_graph, input_node, pass, usage, subresource, color_index, load_op, clear_color);

    RenderGraphResourceNode* output_node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, input_node->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeColorAttchment>(m_graph, pass, output_node, usage, subresource, color_index, load_op, clear_color);

    RenderGraphHandle output;
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}

RenderGraphHandle RenderGraph::WriteDepth(RenderGraphPassBase* pass, const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, GfxRenderPassLoadOp stencil_load_op, float clear_depth, uint32_t clear_stencil)
{
    RE_ASSERT(input.IsValid());
    RenderGraphResource* resource = m_resources[input.index];

    GfxResourceState usage = GfxResourceState::DepthStencil;

    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];
    AllocatePOD<RenderGraphEdgeDepthAttchment>(m_graph, input_node, pass, usage, subresource, depth_load_op, stencil_load_op, clear_depth, clear_stencil);

    RenderGraphResourceNode* output_node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, input_node->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeDepthAttchment>(m_graph, pass, output_node, usage, subresource, depth_load_op, stencil_load_op, clear_depth, clear_stencil);

    RenderGraphHandle output;
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}

RenderGraphHandle RenderGraph::ReadDepth(RenderGraphPassBase* pass, const RenderGraphHandle& input, uint32_t subresource)
{
    RE_ASSERT(input.IsValid());
    RenderGraphResource* resource = m_resources[input.index];

    GfxResourceState usage = GfxResourceState::DepthStencilReadOnly;

    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];
    AllocatePOD<RenderGraphEdgeDepthAttchment>(m_graph, input_node, pass, usage, subresource, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load, 0.0f, 0);

    RenderGraphResourceNode* output_node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, input_node->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeDepthAttchment>(m_graph, pass, output_node, usage, subresource, GfxRenderPassLoadOp::Load, GfxRenderPassLoadOp::Load, 0.0f, 0);

    RenderGraphHandle output;
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}
