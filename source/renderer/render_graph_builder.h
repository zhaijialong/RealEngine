#pragma once

#include "render_graph.h"
#include "gfx/gfx_define.h"

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

    RGHandle Import(IGfxTexture* texture, GfxResourceState state)
    {
        return m_pGraph->Import(texture, state);
    }

    RGHandle Read(const RGHandle& input, GfxResourceState usage, uint32_t subresource = 0)
    {
        RE_ASSERT(usage == GfxResourceState::ShaderResourceNonPS ||
            usage == GfxResourceState::ShaderResourcePS ||
            usage == GfxResourceState::ShaderResourceAll ||
            usage == GfxResourceState::IndirectArg ||
            usage == GfxResourceState::CopySrc ||
            usage == GfxResourceState::ResolveSrc);

        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG doesn't support GFX_ALL_SUB_RESOURCE currently

        return m_pGraph->Read(m_pPass, input, usage, subresource);
    }

    RGHandle Read(const RGHandle& input, uint32_t subresource = 0, RGBuilderFlag flag = RGBuilderFlag::None)
    {
        GfxResourceState state;

        switch (m_pPass->GetType())
        {
        case RenderPassType::Graphics:
            if (flag == RGBuilderFlag::ShaderStagePS)
            {
                state = GfxResourceState::ShaderResourcePS;
            }
            else if (flag == RGBuilderFlag::ShaderStageNonPS)
            {
                state = GfxResourceState::ShaderResourceNonPS;
            }
            else
            {
                state = GfxResourceState::ShaderResourceAll;
            }
            break;
        case RenderPassType::Compute:
        case RenderPassType::AsyncCompute:
            state = GfxResourceState::ShaderResourceNonPS;
            break;
        case RenderPassType::Copy:
            state = GfxResourceState::CopySrc;
            break;
        case RenderPassType::Resolve:
            state = GfxResourceState::ResolveSrc;
            break;
        default:
            RE_ASSERT(false);
            break;
        }
        return Read(input, state, subresource);
    }

    RGHandle ReadIndirectArg(const RGHandle& input, uint32_t subresource = 0)
    {
        return Read(input, GfxResourceState::IndirectArg, subresource);
    }

    RGHandle Write(const RGHandle& input, GfxResourceState usage, uint32_t subresource = 0)
    {
        RE_ASSERT(usage == GfxResourceState::UnorderedAccess ||
            usage == GfxResourceState::CopyDst ||
            usage == GfxResourceState::ResolveDst);

        RE_ASSERT(GFX_ALL_SUB_RESOURCE != subresource); //RG doesn't support GFX_ALL_SUB_RESOURCE currently

        return m_pGraph->Write(m_pPass, input, usage, subresource);
    }

    RGHandle Write(const RGHandle& input, uint32_t subresource = 0)
    {
        GfxResourceState state;

        switch (m_pPass->GetType())
        {
        case RenderPassType::Graphics:
        case RenderPassType::Compute:
        case RenderPassType::AsyncCompute:
            state = GfxResourceState::UnorderedAccess;
            break;
        case RenderPassType::Copy:
            state = GfxResourceState::CopyDst;
            break;
        case RenderPassType::Resolve:
            state = GfxResourceState::ResolveDst;
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
