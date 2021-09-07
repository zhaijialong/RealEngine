#pragma once

#include "render_graph.h"
#include "gfx/gfx_define.h"

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph* graph, RenderGraphPassBase* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    void MakeTarget() { m_pPass->MakeTarget(); }

    template<typename Resource>
    RenderGraphHandle Create(const typename Resource::Desc& desc, const char* name)
    {
        return m_pGraph->Create<Resource>(desc, name);
    }

    RenderGraphHandle Read(const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource = GFX_ALL_SUB_RESOURCE)
    {
        return m_pGraph->Read(m_pPass, input, usage, subresource);
    }

    RenderGraphHandle Write(const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource = GFX_ALL_SUB_RESOURCE)
    {
        return m_pGraph->Write(m_pPass, input, usage, subresource);
    }

private:
    RenderGraphBuilder(RenderGraphBuilder const&) = delete;
    RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    RenderGraphPassBase* m_pPass = nullptr;
};