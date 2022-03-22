#pragma once

#include "render_graph_pass.h"
#include "render_graph_handle.h"
#include "render_graph_resource.h"
#include "render_graph_resource_allocator.h"
#include "utils/linear_allocator.h"
#include "utils/math.h"
#include "EASTL/unique_ptr.h"

class RenderGraphResourceNode;
class Renderer;

class RenderGraph
{
    friend class RenderGraphBuilder;
public:
    RenderGraph(Renderer* pRenderer);

    template<typename Data, typename Setup, typename Exec>
    RenderGraphPass<Data>& AddPass(const eastl::string& name, RenderPassType type, const Setup& setup, const Exec& execute);

    void BeginEvent(const eastl::string& name);
    void EndEvent();

    void Clear();
    void Compile();
    void Execute(IGfxCommandList* pCommandList, IGfxCommandList* pComputeCommandList);

    void Present(const RenderGraphHandle& handle, GfxResourceState filnal_state);

    RenderGraphHandle Import(IGfxTexture* texture, GfxResourceState state);
    RenderGraphHandle Import(IGfxBuffer* buffer, GfxResourceState state);

    RenderGraphResource* GetResource(const RenderGraphHandle& handle);
    const DirectedAcyclicGraph& GetDAG() const { return m_graph; }

    bool Export(const eastl::string& file);

private:
    template<typename T, typename... ArgsT>
    T* Allocate(ArgsT&&... arguments);

    template<typename T, typename... ArgsT>
    T* AllocatePOD(ArgsT&&... arguments);

    template<typename Resource>
    RenderGraphHandle Create(const typename Resource::Desc& desc, const eastl::string& name);

    RenderGraphHandle Read(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource);
    RenderGraphHandle Write(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource);

    RenderGraphHandle WriteColor(RenderGraphPassBase* pass, uint32_t color_index, const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp load_op, const float4& clear_color);
    RenderGraphHandle WriteDepth(RenderGraphPassBase* pass, const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, GfxRenderPassLoadOp stencil_load_op, float clear_depth, uint32_t clear_stencil);
    RenderGraphHandle ReadDepth(RenderGraphPassBase* pass, const RenderGraphHandle& input, uint32_t subresource);

private:
    LinearAllocator m_allocator { 512 * 1024 };
    RenderGraphResourceAllocator m_resourceAllocator;
    DirectedAcyclicGraph m_graph;

    eastl::vector<eastl::string> m_eventNames;

    eastl::unique_ptr<IGfxFence> m_pAsyncComputeFence;
    uint64_t m_nAsyncComputeFenceValue = 0;

    eastl::vector<RenderGraphPassBase*> m_passes;
    eastl::vector<RenderGraphResource*> m_resources;
    eastl::vector<RenderGraphResourceNode*> m_resourceNodes;

    struct ObjFinalizer
    {
        void* obj;
        void(*finalizer)(void*);
    };
    eastl::vector<ObjFinalizer>  m_objFinalizer;

    struct PresentTarget
    {
        RenderGraphResource* resource;
        GfxResourceState state;
    };
    eastl::vector<PresentTarget> m_outputResources;
};

class RenderGraphEvent
{
public:
    RenderGraphEvent(RenderGraph* graph, const char* name) : m_pRenderGraph(graph)
    {
        m_pRenderGraph->BeginEvent(name);
    }

    ~RenderGraphEvent()
    {
        m_pRenderGraph->EndEvent();
    }

private:
    RenderGraph* m_pRenderGraph;
};

#define RENDER_GRAPH_EVENT(graph, event_name) RenderGraphEvent __graph_event__(graph, event_name)

#include "render_graph.inl"