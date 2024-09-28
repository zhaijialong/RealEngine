#include "motion_blur.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "motion_blur/motion_blur_common.hlsli"

// references :
// "A Reconstruction Filter for Plausible Motion Blur", Morgan McGuire, 2012
// "Next Generation Post Processing in Call of Duty: Advanced Warfare", Jorge Jimenez, 2014

MotionBlur::MotionBlur(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("motion_blur/pack_velocity_depth.hlsl", "main", GfxShaderType::CS);
    m_pPackVelocityPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur Pack Velocity PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/tile_velocity.hlsl", "main", GfxShaderType::CS);
    m_pTileVelocityXPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur TileVelocity X PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/tile_velocity.hlsl", "main", GfxShaderType::CS, { "VERTICAL_PASS=1"});
    m_pTileVelocityYPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur TileVelocity Y PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/neighbor_max.hlsl", "main", GfxShaderType::CS);
    m_pNeighborMaxPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur NeighborMax PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/reconstruction_filter.hlsl", "main", GfxShaderType::CS);
    m_pReconstructionPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur Reconstruction PSO");

    psoDesc.cs = pRenderer->GetShader("motion_blur/reconstruction_filter.hlsl", "main", GfxShaderType::CS, { "MOTION_BLUR_DEBUG=1" });
    m_pReconstructionDebugPSO = pRenderer->GetPipelineState(psoDesc, "MotionBlur Reconstruction PSO");
}

void MotionBlur::OnGui()
{
    if (ImGui::CollapsingHeader("Motion Blur"))
    {
        ImGui::Checkbox("Enable##MotionBlur", &m_bEnable);
        ImGui::SliderInt("Sample Count##MotionBlur", (int*)&m_sampleCount, 10, 20);
        ImGui::SliderFloat("Min Velocity Length##MotionBlur", &m_minVelocityLength, 0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat("Max Velocity Length##MotionBlur", &m_maxVelocityLength, 20.0f, 200.0f, "%.1f");
        ImGui::Checkbox("Debug##MotionBlur", &m_bDebug);
    }
}

RGHandle MotionBlur::AddPass(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth, RGHandle velocity, uint32_t width, uint32_t height)
{
    if (!m_bEnable)
    {
        return sceneColor;
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "Motion Blur");

    struct PackVelocityDepthPassData
    {
        RGHandle velocity;
        RGHandle depth;
        RGHandle output;
    };

    auto pack_velocity_depth = pRenderGraph->AddPass<PackVelocityDepthPassData>("PackVelocityDepth", RenderPassType::Compute,
        [&](PackVelocityDepthPassData& data, RGBuilder& builder)
        {
            data.velocity = builder.Read(velocity);
            data.depth = builder.Read(sceneDepth);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur PackedVelocity"));
        },
        [=](const PackVelocityDepthPassData& data, IGfxCommandList* pCommandList)
        {
            PackVelocityDepth(pCommandList,
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.depth),
                pRenderGraph->GetTexture(data.output));
        });

    struct TileVelocityPassData
    {
        RGHandle input;
        RGHandle output;
    };

    auto tile_velocity_x = pRenderGraph->AddPass<TileVelocityPassData>("TileVelocity - X", RenderPassType::Compute,
        [&](TileVelocityPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(pack_velocity_depth->output);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, MOTION_BLUR_TILE_SIZE);
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur TileVelocityX"));
        },
        [=](const TileVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            TileMinMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output), false);
        });

    auto tile_velocity_y = pRenderGraph->AddPass<TileVelocityPassData>("TileVelocity - Y", RenderPassType::Compute,
        [&](TileVelocityPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(tile_velocity_x->output);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, MOTION_BLUR_TILE_SIZE);
            desc.height = DivideRoudingUp(height, MOTION_BLUR_TILE_SIZE);
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur TileVelocityY"));
        },
        [=](const TileVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            TileMinMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output), true);
        });

    auto neighbor_max = pRenderGraph->AddPass<TileVelocityPassData>("NeighborMax", RenderPassType::Compute,
        [&](TileVelocityPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(tile_velocity_y->output);

            RGTexture::Desc desc;
            desc.width = DivideRoudingUp(width, MOTION_BLUR_TILE_SIZE);
            desc.height = DivideRoudingUp(height, MOTION_BLUR_TILE_SIZE);
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "MotionBlur NeighborMax"));
        },
        [=](const TileVelocityPassData& data, IGfxCommandList* pCommandList)
        {
            NeighborMax(pCommandList, pRenderGraph->GetTexture(data.input), pRenderGraph->GetTexture(data.output));
        });

    struct ReconstructionPassData
    {
        RGHandle color;
        RGHandle velocityDepth;
        RGHandle neighborMax;
        RGHandle output;
    };

    auto reconstruction_filter = pRenderGraph->AddPass<ReconstructionPassData>("Reconstruction", RenderPassType::Compute,
        [&](ReconstructionPassData& data, RGBuilder& builder)
        {
            data.color = builder.Read(sceneColor);
            data.velocityDepth = builder.Read(pack_velocity_depth->output);
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
                pRenderGraph->GetTexture(data.velocityDepth),
                pRenderGraph->GetTexture(data.neighborMax),
                pRenderGraph->GetTexture(data.output));
        });

    return reconstruction_filter->output;
}

void MotionBlur::PackVelocityDepth(IGfxCommandList* pCommandList, RGTexture* velocity, RGTexture* depth, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pPackVelocityPSO);

    struct CB
    {
        uint velocityTexture;
        uint depthTexture;
        uint outputTexture;
        float minVelocityLength;
        float maxVelocityLength;
    };

    CB cb;
    cb.velocityTexture = velocity->GetSRV()->GetHeapIndex();
    cb.depthTexture = depth->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();
    cb.minVelocityLength = m_minVelocityLength;
    cb.maxVelocityLength = m_maxVelocityLength;

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void MotionBlur::TileMinMax(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, bool vertical_pass)
{
    pCommandList->SetPipelineState(vertical_pass ? m_pTileVelocityYPSO : m_pTileVelocityXPSO);

    struct CB
    {
        uint inputTexture;
        uint outputTexture;
        uint2 inputTextureSize;
    };

    CB cb;
    cb.inputTexture = input->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();
    cb.inputTextureSize.x = input->GetTexture()->GetDesc().width;
    cb.inputTextureSize.y = input->GetTexture()->GetDesc().height;

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

void MotionBlur::ReconstructionFilter(IGfxCommandList* pCommandList, RGTexture* color, RGTexture* velocityDepth, RGTexture* neighborMax, RGTexture* output)
{
    pCommandList->SetPipelineState(m_bDebug ? m_pReconstructionDebugPSO : m_pReconstructionPSO);

    struct CB
    {
        uint colorTexture;
        uint velocityDepthTexture;
        uint neighborMaxTexture;
        uint outputTexture;

        uint sampleCount;
    };

    CB cb;
    cb.colorTexture = color->GetSRV()->GetHeapIndex();
    cb.velocityDepthTexture = velocityDepth->GetSRV()->GetHeapIndex();
    cb.neighborMaxTexture = neighborMax->GetSRV()->GetHeapIndex();
    cb.outputTexture = output->GetUAV()->GetHeapIndex();
    cb.sampleCount = m_sampleCount;

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
