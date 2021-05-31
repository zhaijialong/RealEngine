#pragma once

#include "render_graph_resource.h"

class RenderGraph;
class RenderPass;

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, RenderPass* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    RenderGraphResource* Read(RenderGraphResource* resource);
    RenderGraphResource* Write(RenderGraphResource* resource);

    RenderGraphTexture* CreateTexture(const std::string& name, uint32_t width, uint32_t height);
    //todo : RenderGraphBuffer* CreateBuffer(const std::string& name);

private:
    RenderGraph* m_pGraph = nullptr;
    RenderPass* m_pPass = nullptr;
};