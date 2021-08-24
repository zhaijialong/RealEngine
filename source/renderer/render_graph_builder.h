#pragma once

#include "directed_acyclic_graph.h"
#include "render_graph_handle.h"

class RenderGraph;

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, DAGNode* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    void MakeTarget() { m_pPass->MakeTarget(); }

    RenderGraphHandle Create();
    RenderGraphHandle Read(RenderGraphHandle input /*, usage */);
    RenderGraphHandle Write(RenderGraphHandle input /*, usage */);

private:
    RenderGraphBuilder(RenderGraphBuilder const&) = delete;
    RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    DAGNode* m_pPass = nullptr;
};