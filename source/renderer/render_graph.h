#pragma once

#include "render_graph_pass.h"
#include "render_graph_handle.h"
#include "render_graph_resource.h"
#include "render_graph_blackboard.h"

class RenderGraphResourceNode;

class RenderGraph
{
    friend class RenderGraphBuilder;
public:
    template<typename Data, typename Setup, typename Exec>
    RenderGraphPass<Data>& AddPass(const char* name, const Setup& setup, const Exec& execute);

    void Clear();
    void Compile();
    void Execute();

    void Present(const RenderGraphHandle& handle);

private:
    template<typename Resource>
    RenderGraphHandle Create(const char* name, const typename Resource::Desc& desc);

    RenderGraphHandle Read(DAGNode* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource);
    RenderGraphHandle Write(DAGNode* pass, const RenderGraphHandle& output, GfxResourceState usage, uint32_t subresource);

private:
    //todo : LinearAllocator m_allocator;

    DirectedAcyclicGraph m_graph;
    RenderGraphBlackboard m_blackboard;

    std::vector<RenderGraphResource*> m_resources;
    std::vector<RenderGraphResourceNode*> m_resourceNodes;
};

#include "render_graph.inl"