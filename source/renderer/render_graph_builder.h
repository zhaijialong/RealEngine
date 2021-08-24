#pragma once

#include "directed_acyclic_graph.h"

class RenderGraph;

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, DAGNode* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    //RenderGraphResource* Read(RenderGraphResource* resource);
    //RenderGraphResource* Write(RenderGraphResource* resource);

    //RenderGraphTexture* CreateTexture(const std::string& name, uint32_t width, uint32_t height);
    //todo : RenderGraphBuffer* CreateBuffer(const std::string& name);

private:
    RenderGraph* m_pGraph = nullptr;
    DAGNode* m_pPass = nullptr;
};