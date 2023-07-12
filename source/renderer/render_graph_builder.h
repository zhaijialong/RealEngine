#pragma once

#include "render_graph.h"
#include "gfx/gfx_defines.h"

enum class RGBuilderFlag
{
    None,
    ShaderStagePS,
    ShaderStageNonPS,
};

class RGBuilder
{
public:
    RGBuilder(RenderGraph* graph, RenderGraphPassBase* pass)
    {
        m_pGraph = graph;
        m_pPass = pass;
    }

    void SkipCulling() { m_pPass->MakeTarget(); }

    template<typename Resource>
    RGHandle Create(const typename Resource::Desc& desc, const eastl::string& name)
    {
        return m_pGraph->Create<Resource>(desc, name);
    }

    RGHandle Import(IGfxTexture* texture, GfxAccessFlags state)
    {
        return m_pGraph->Import(texture, state);
    }

    RGHandle Read(const RGHandle& input, GfxAccessFlags usage, uint32_t subresource)
    {
        RE_ASSERT(usage & (GfxAccessMaskSRV | GfxAccessIndirectArgs | GfxAccessCopySrc));

        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG doesn't support GFX_ALL_SUB_RESOURCE currently

        return m_pGraph->Read(m_pPass, input, usage, subresource);
    }

    RGHandle Read(const RGHandle& input, uint32_t subresource = 0, RGBuilderFlag flag = RGBuilderFlag::None)
    {
        GfxAccessFlags state;

        switch (m_pPass->GetType())
        {
        case RenderPassType::Graphics:
            if (flag == RGBuilderFlag::ShaderStagePS)
            {
                state = GfxAccessPixelShaderSRV;
            }
            else if (flag == RGBuilderFlag::ShaderStageNonPS)
            {
                state = GfxAccessVertexShaderSRV;
            }
            else
            {
                state = GfxAccessPixelShaderSRV | GfxAccessVertexShaderSRV;
            }
            break;
        case RenderPassType::Compute:
        case RenderPassType::AsyncCompute:
            state = GfxAccessComputeSRV;
            break;
        case RenderPassType::Copy:
            state = GfxAccessCopySrc;
            break;
        default:
            RE_ASSERT(false);
            break;
        }
        return Read(input, state, subresource);
    }

    RGHandle ReadIndirectArg(const RGHandle& input, uint32_t subresource = 0)
    {
        return Read(input, GfxAccessIndirectArgs, subresource);
    }

    RGHandle Write(const RGHandle& input, GfxAccessFlags usage, uint32_t subresource)
    {
        RE_ASSERT(usage & (GfxAccessMaskUAV | GfxAccessCopyDst));

        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG doesn't support GFX_ALL_SUB_RESOURCE currently

        return m_pGraph->Write(m_pPass, input, usage, subresource);
    }

    RGHandle Write(const RGHandle& input, uint32_t subresource = 0, RGBuilderFlag flag = RGBuilderFlag::None)
    {
        GfxAccessFlags state;

        switch (m_pPass->GetType())
        {
        case RenderPassType::Graphics:
            if (flag == RGBuilderFlag::ShaderStagePS)
            {
                state = GfxAccessPixelShaderUAV;
            }
            else if (flag == RGBuilderFlag::ShaderStageNonPS)
            {
                state = GfxAccessVertexShaderUAV;
            }
            else
            {
                state = GfxAccessPixelShaderUAV | GfxAccessVertexShaderUAV;
            }
            break;
        case RenderPassType::Compute:
        case RenderPassType::AsyncCompute:
            state = GfxAccessComputeUAV;
            break;
        case RenderPassType::Copy:
            state = GfxAccessCopyDst;
            break;
        default:
            RE_ASSERT(false);
            break;
        }
        return Write(input, state, subresource);
    }

    RGHandle WriteColor(uint32_t color_index, const RGHandle& input, uint32_t subresource, GfxRenderPassLoadOp load_op, float4 clear_color = float4(0.0f, 0.0f, 0.0f, 1.0f))
    {
        RE_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteColor(m_pPass, color_index, input, subresource, load_op, clear_color);
    }

    RGHandle WriteDepth(const RGHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, float clear_depth = 0.0f)
    {
        RE_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteDepth(m_pPass, input, subresource, depth_load_op, GfxRenderPassLoadOp::DontCare, clear_depth, 0);
    }

    RGHandle WriteDepth(const RGHandle& input, uint32_t subresource, GfxRenderPassLoadOp depth_load_op, GfxRenderPassLoadOp stencil_load_op, float clear_depth = 0.0f, uint32_t clear_stencil = 0)
    {
        RE_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteDepth(m_pPass, input, subresource, depth_load_op, stencil_load_op, clear_depth, clear_stencil);
    }

    RGHandle ReadDepth(const RGHandle& input, uint32_t subresource)
    {
        RE_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->ReadDepth(m_pPass, input, subresource);
    }
private:
    RGBuilder(RGBuilder const&) = delete;
    RGBuilder& operator=(RGBuilder const&) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    RenderGraphPassBase* m_pPass = nullptr;
};
