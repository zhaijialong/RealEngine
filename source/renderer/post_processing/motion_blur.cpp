#include "motion_blur.h"
#include "../renderer.h"
#include "utils/gui_util.h"

// references :
// "A Reconstruction Filter for Plausible Motion Blur", Morgan McGuire, 2012

MotionBlur::MotionBlur(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("motion_blur/tile_max.hlsl", "main", "cs_6_6", {});
    m_pTileMaxXPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur TileMax X PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/tile_max.hlsl", "main", "cs_6_6", { "VERTICAL_PASS=1"});
    m_pTileMaxYPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur TileMax Y PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/neighbor_max.hlsl", "main", "cs_6_6", {});
    m_pNeighborMaxPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur NeighborMax PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/reconstruction_filter.hlsl", "main", "cs_6_6", {});
    m_pReconstructionPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur Reconstruction PSO");
}

RGHandle MotionBlur::Render(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth, RGHandle velocity, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "Motion Blur",
        [&]()
        {
            ImGui::Checkbox("Enable##MotionBlur", &m_bEnable);
            ImGui::SliderInt("Max Blur Radius##MotionBlur", (int*)&m_maxBlurRadius, 8, 32);
            ImGui::SliderInt("Sample Count##MotionBlur", (int*)&m_sampleCount, 0, 20);
        });

    if (!m_bEnable)
    {
        return sceneColor;
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "Motion Blur");

    const uint32_t K = m_maxBlurRadius;

    struct TileMaxPassData
    {
        RGHandle input;
        RGHandle output;
    };

    auto tile_max_x = pRenderGraph->AddPass<TileMaxPassData>("TileMax - X", RenderPassType::Compute,
        [&](TileMaxPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(velocity);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, K);
            desc.height = height;
            desc.format = GfxFormat::RG16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur TileMaxX"));
        },
        [=](const TileMaxPassData& data, IGfxCommandList* pCommandList)
        {
            TileMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output), false);
        });

    auto tile_max_y = pRenderGraph->AddPass<TileMaxPassData>("TileMax - Y", RenderPassType::Compute,
        [&](TileMaxPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(tile_max_x->output);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, K);
            desc.height = DivideRoudingUp(height, K);
            desc.format = GfxFormat::RG16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur TileMaxY"));
        },
        [=](const TileMaxPassData& data, IGfxCommandList* pCommandList)
        {
            TileMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output), true);
        });

    auto neighbor_max = pRenderGraph->AddPass<TileMaxPassData>("NeighborMax", RenderPassType::Compute,
        [&](TileMaxPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(tile_max_y->output);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, K);
            desc.height = DivideRoudingUp(height, K);
            desc.format = GfxFormat::RG16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur NeighborMax"));
        },
        [=](const TileMaxPassData& data, IGfxCommandList* pCommandList)
        {
            NeighborMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output));
        });

    struct ReconstructionPassData
    {
        RGHandle color;
        RGHandle depth;
        RGHandle velocity;
        RGHandle neighborMax;
        RGHandle output;
    };

    auto reconstruction_filter = pRenderGraph->AddPass<ReconstructionPassData>("Reconstruction", RenderPassType::Compute,
        [&](ReconstructionPassData& data, RGBuilder& builder)
        {
            data.color = builder.Read(sceneColor);
            data.depth = builder.Read(sceneDepth);
            data.velocity = builder.Read(velocity);
            data.neighborMax = builder.Read(neighbor_max->output);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur Output"));
        },
        [=](const ReconstructionPassData& data, IGfxCommandList* pCommandList)
        {
            ReconstructionFilter(pCommandList,
            pRenderGraph->GetTexture(data.color),
            pRenderGraph->GetTexture(data.depth),
            pRenderGraph->GetTexture(data.velocity),
            pRenderGraph->GetTexture(data.neighborMax),
            pRenderGraph->GetTexture(data.output));
        });

    return reconstruction_filter->output;
}

void MotionBlur::TileMax(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, bool vertical_pass)
{
    pCommandList->SetPipelineState(vertical_pass ? m_pTileMaxYPSO : m_pTileMaxXPSO);

    struct CB
    {
        uint inputTexture;
        uint outputTexture;
        float2 rcpTextureSize;
        uint tileSize;
    };

    CB cb;
    cb.inputTexture = input->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();
    cb.rcpTextureSize = float2(1.0f / input->GetTexture()->GetDesc().width, 1.0f / input->GetTexture()->GetDesc().height);
    cb.tileSize = m_maxBlurRadius;

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void MotionBlur::NeighborMax(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pNeighborMaxPSO);

    struct CB
    {
        uint inputTexture;
        uint outputTexture;
    };

    CB cb;
    cb.inputTexture = input->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void MotionBlur::ReconstructionFilter(IGfxCommandList* pCommandList, RGTexture* color, RGTexture* depth, RGTexture* velocity, RGTexture* neighborMax, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pReconstructionPSO);

    struct CB
    {
        uint colorTexture;
        uint depthTexture;
        uint velocityTexture;
        uint neighborMaxTexture;

        uint outputTexture;
        uint tileSize;
        uint sampleCount;
    };

    CB cb;
    cb.colorTexture = color->GetSRV()->GetHeapIndex();
    cb.depthTexture = depth->GetSRV()->GetHeapIndex();
    cb.velocityTexture = velocity->GetSRV()->GetHeapIndex();
    cb.neighborMaxTexture = neighborMax->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();
    cb.tileSize = m_maxBlurRadius;
    cb.sampleCount = m_sampleCount;

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
