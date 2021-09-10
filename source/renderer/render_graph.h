#pragma once

#include "render_graph_pass.h"
#include "render_graph_handle.h"
#include "render_graph_resource.h"
#include "render_graph_blackboard.h"
#include "utils/linear_allocator.h"

class RenderGraphResourceNode;
class Renderer;

class RenderGraph
{
    friend class RenderGraphBuilder;
public:
    RenderGraph(Renderer* pRenderer);

    template<typename Data, typename Setup, typename Exec>
    RenderGraphPass<Data>& AddPass(const char* name, const Setup& setup, const Exec& execute);

    void Clear();
    void Compile();
    void Execute(IGfxCommandList* pCommandList);

    void Present(const RenderGraphHandle& handle);
    RenderGraphResource* GetResource(const RenderGraphHandle& handle);

    bool Export(const std::string& file);

private:
    template<typename T, typename... ArgsT>
    T* Allocate(ArgsT&&... arguments);

    template<typename T, typename... ArgsT>
    T* AllocatePOD(ArgsT&&... arguments);

    template<typename Resource>
    RenderGraphHandle Create(const typename Resource::Desc& desc, const char* name);

    RenderGraphHandle Read(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource);
    RenderGraphHandle Write(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource);

private:
    LinearAllocator m_allocator { 32 * 1024 };
    RenderGraphResourceAllocator m_resourceAllocator;
    DirectedAcyclicGraph m_graph;
    RenderGraphBlackboard m_blackboard;

    std::vector<RenderGraphPassBase*> m_passes;
    std::vector<RenderGraphResource*> m_resources;
    std::vector<RenderGraphResourceNode*> m_resourceNodes;

    struct ObjFinalizer
    {
        void* obj;
        void(*finalizer)(void*);
    };
    std::vector<ObjFinalizer>  m_objFinalizer;

    RenderGraphResource* m_pOutputResource = nullptr;
};

#include "render_graph.inl"