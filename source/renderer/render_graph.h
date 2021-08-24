#pragma once

#include "render_graph_pass.h"
#include "render_graph_builder.h"

class RenderGraph
{
public:
    template<typename Data, typename Setup, typename Exec>
    RenderGraphPass<Data>& AddPass(const char* name, const Setup& setup, const Exec& execute);

    void Clear();
    void Compile();
    void Execute();

private:
    //todo : LinearAllocator m_allocator;

    DirectedAcyclicGraph m_graph;
};

template<typename Data, typename Setup, typename Exec>
inline RenderGraphPass<Data>& RenderGraph::AddPass(const char* name, const Setup& setup, const Exec& execute)
{
    auto* pass = new RenderGraphPass<Data>(name, m_graph, execute);

    RenderGraphBuilder builder(this, pass);
    setup(pass->GetData(), builder);

    return *pass;
}
