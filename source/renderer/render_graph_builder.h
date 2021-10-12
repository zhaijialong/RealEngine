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

    RenderGraphHandle Read(const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource = 0)
    {
        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG目前不支持GFX_ALL_SUB_RESOURCE
        return m_pGraph->Read(m_pPass, input, usage, subresource);
    }

    RenderGraphHandle Write(const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource = 0)
    {
        //use WriteColor/WriteDepth
        RE_ASSERT(usage != GfxResourceState::RenderTarget && usage != GfxResourceState::DepthStencil && usage != GfxResourceState::DepthStencilReadOnly);

        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG目前不支持GFX_ALL_SUB_RESOURCE
        return m_pGraph->Write(m_pPass, input, usage, subresource);
    }

    RenderGraphHandle WriteColor(uint32_t color_index, const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp load_op, float4 clear_color = float4(0.0f, 0.0f, 0.0f, 1.0f))
    {
        return m_pGraph->WriteColor(m_pPass, color_index, input, subresource, load_op, clear_color);
    }

    RenderGraphHandle WriteDepth(const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, float clear_depth = 0.0f, bool read_only = false)
    {
        return m_pGraph->WriteDepth(m_pPass, input, subresource, read_only, depth_load_op, GfxRenderPassLoadOp::DontCare, clear_depth, 0);
    }

    RenderGraphHandle WriteDepth(const RenderGraphHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, GfxRenderPassLoadOp stencil_load_op, float clear_depth = 0.0f, uint32_t clear_stencil = 0, bool read_only = false)
    {
        return m_pGraph->WriteDepth(m_pPass, input, subresource, read_only, depth_load_op, stencil_load_op, clear_depth, clear_stencil);
    }

private:
    RenderGraphBuilder(RenderGraphBuilder const&) = delete;
    RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    RenderGraphPassBase* m_pPass = nullptr;
};