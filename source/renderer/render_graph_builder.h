#pragma once

#include "render_graph.h"

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, DAGNode* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    void MakeTarget() { m_pPass->MakeTarget(); }

    template<typename Resource>
    RenderGraphHandle Create(const char* name, const typename Resource::Desc& desc)
    {
        return m_pGraph->Create<Resource>(name, desc);
    }

    RenderGraphHandle Read(RenderGraphHandle input /*, usage */);
    RenderGraphHandle Write(RenderGraphHandle input /*, usage */);

private:
    RenderGraphBuilder(RenderGraphBuilder const&) = delete;
    RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    DAGNode* m_pPass = nullptr;
};