#include "dof.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "dof/dof_common.hlsli"

DoF::DoF(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("dof/downsample.hlsl", "main", GfxShaderType::CS, {});
    m_pDownsamplePSO = pRenderer->GetPipelineState(psoDesc, "DOF - downsample PSO");

    psoDesc.cs = pRenderer->GetShader("dof/circular_filter_far.hlsl", "main_horizontal", GfxShaderType::CS, {});
    m_pFarHorizontalBlurPSO = pRenderer->GetPipelineState(psoDesc, "DOF - far blur X PSO");

    psoDesc.cs = pRenderer->GetShader("dof/circular_filter_far.hlsl", "main_vertical", GfxShaderType::CS, {});
    m_pFarVerticalBlurPSO = pRenderer->GetPipelineState(psoDesc, "DOF - far blur Y PSO");

    psoDesc.cs = pRenderer->GetShader("dof/tile_coc.hlsl", "main", GfxShaderType::CS, {});
    m_pTileCocXPSO = pRenderer->GetPipelineState(psoDesc, "DOF - tile coc dilate PSO");

    psoDesc.cs = pRenderer->GetShader("dof/tile_coc.hlsl", "main", GfxShaderType::CS, { "VERTICAL_PASS=1" });
    m_pTileCocYPSO = pRenderer->GetPipelineState(psoDesc, "DOF - tile coc dilate PSO");

    psoDesc.cs = pRenderer->GetShader("dof/circular_filter_near.hlsl", "main_horizontal", GfxShaderType::CS, {});
    m_pNearHorizontalBlurPSO = pRenderer->GetPipelineState(psoDesc, "DOF - near blur X PSO");

    psoDesc.cs = pRenderer->GetShader("dof/circular_filter_near.hlsl", "main_vertical", GfxShaderType::CS, {});
    m_pNearVerticalBlurPSO = pRenderer->GetPipelineState(psoDesc, "DOF - near blur Y PSO");

    psoDesc.cs = pRenderer->GetShader("dof/composite.hlsl", "main", GfxShaderType::CS, {});
    m_pCompositePSO = pRenderer->GetPipelineState(psoDesc, "DOF - composite PSO");
}

void DoF::OnGui()
{
    if (ImGui::CollapsingHeader("DoF"))
    {
        ImGui::Checkbox("Enable##DoF", &m_bEnable);
        ImGui::SliderFloat("Focus Distance##DoF", &m_focusDistance, 0.001f, 100.0f);
        ImGui::SliderFloat("Max CoC Size##DoF", &m_maxCocSize, 0.001f, 32.0f);
    }
}

RGHandle DoF::AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height)
{
    if (!m_bEnable)
    {
        return color;
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "DoF");

    uint32_t halfWidth = DivideRoudingUp(width, 2);
    uint32_t halfHeight = DivideRoudingUp(height, 2);

    RGHandle far, near;
    AddDownsamplePass(pRenderGraph, color, depth, halfWidth, halfHeight, far, near);

    RGHandle farBlur = AddFarBlurPass(pRenderGraph, far, halfWidth, halfHeight);
    RGHandle nearCoc = AddTileCocPass(pRenderGraph, near, halfWidth, halfHeight);
    RGHandle nearBlur = AddNearBlurPass(pRenderGraph, near, nearCoc, halfWidth, halfHeight);
    RGHandle output = AddCompositePass(pRenderGraph, color, depth, farBlur, nearBlur, width, height);

    return output;
}

void DoF::AddDownsamplePass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height, RGHandle& downsampledFar, RGHandle& downsampledNear)
{
    struct DownsamplePassData
    {
        RGHandle color;
        RGHandle depth;
        RGHandle downsampledFar;
        RGHandle downsampledNear;
    };

    auto downsamplePass = pRenderGraph->AddPass<DownsamplePassData>("Downsample", RenderPassType::Compute,
        [&](DownsamplePassData& data, RGBuilder& builder)
        {
            data.color = builder.Read(color);
            data.depth = builder.Read(depth);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.downsampledFar = builder.Write(builder.Create<RGTexture>(desc, "DoF - downsampled far color/coc"));
            data.downsampledNear = builder.Write(builder.Create<RGTexture>(desc, "DoF - downsampled near color/coc"));
        },
        [=](const DownsamplePassData& data, IGfxCommandList* pCommandList)
        {
            struct CB
            {
                uint colorTexture;
                uint depthTexture;
                uint downsampledFarTexture;
                uint downsampledNearTexture;

                float focusDistance;
                float maxCocSize;
            };

            CB cb = {
                pRenderGraph->GetTexture(data.color)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.depth)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.downsampledFar)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.downsampledNear)->GetUAV()->GetHeapIndex(),

                m_focusDistance,
                m_maxCocSize,
            };

            pCommandList->SetPipelineState(m_pDownsamplePSO);
            pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    downsampledFar = downsamplePass->downsampledFar;
    downsampledNear = downsamplePass->downsampledNear;
}

RGHandle DoF::AddFarBlurPass(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height)
{
    struct FarBlurXPassData
    {
        RGHandle input;
        RGHandle outputR;
        RGHandle outputG;
        RGHandle outputB;
        RGHandle outputWeights;
    };

    auto horizontalPass = pRenderGraph->AddPass<FarBlurXPassData>("FarBlur X", RenderPassType::Compute,
        [&](FarBlurXPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.outputR = builder.Write(builder.Create<RGTexture>(desc, "DoF - FarBlur R"));
            data.outputG = builder.Write(builder.Create<RGTexture>(desc, "DoF - FarBlur G"));
            data.outputB = builder.Write(builder.Create<RGTexture>(desc, "DoF - FarBlur B"));

            desc.format = GfxFormat::R16F;
            data.outputWeights = builder.Write(builder.Create<RGTexture>(desc, "DoF - FarBlur Weights"));
        },
        [=](const FarBlurXPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputR)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputG)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputB)->GetUAV()->GetHeapIndex(),

                0,
                pRenderGraph->GetTexture(data.outputWeights)->GetUAV()->GetHeapIndex(),
                asuint(m_maxCocSize),
            };

            pCommandList->SetPipelineState(m_pFarHorizontalBlurPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    struct FarBlurYPassData
    {
        RGHandle input;
        RGHandle R;
        RGHandle G;
        RGHandle B;
        RGHandle weights;
        RGHandle output;
    };

    auto verticalPass = pRenderGraph->AddPass<FarBlurYPassData>("FarBlur Y", RenderPassType::Compute,
        [&](FarBlurYPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.R = builder.Read(horizontalPass->outputR);
            data.G = builder.Read(horizontalPass->outputG);
            data.B = builder.Read(horizontalPass->outputB);
            data.weights = builder.Read(horizontalPass->outputWeights);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.output = builder.Write(builder.Create<RGTexture>(desc, "DoF - FarBlur"));
        },
        [=](const FarBlurYPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.R)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.G)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.B)->GetSRV()->GetHeapIndex(),

                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.weights)->GetSRV()->GetHeapIndex(),
                asuint(m_maxCocSize),
            };

            pCommandList->SetPipelineState(m_pFarVerticalBlurPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    return verticalPass->output;
}

RGHandle DoF::AddTileCocPass(RenderGraph* pRenderGraph, RGHandle input, uint32_t width, uint32_t height)
{
    struct TileDilationPassData
    {
        RGHandle input;
        RGHandle output;
    };

    uint32_t tiledSizeX = DivideRoudingUp(width, NEAR_COC_TILE_SIZE);
    uint32_t tiledSizeY = DivideRoudingUp(height, NEAR_COC_TILE_SIZE);

    auto horizontalPass = pRenderGraph->AddPass<TileDilationPassData>("Tile Coc X", RenderPassType::Compute,
        [&](TileDilationPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);

            RGTexture::Desc desc;
            desc.width = tiledSizeX;
            desc.height = height;
            desc.format = GfxFormat::R16F;

            data.output = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearCoc Tiled temp"));
        },
        [=](const TileDilationPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
            };

            pCommandList->SetPipelineState(m_pTileCocXPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(tiledSizeX, 8), DivideRoudingUp(height, 8), 1);
        });

    auto verticalPass = pRenderGraph->AddPass<TileDilationPassData>("Tile Coc Y", RenderPassType::Compute,
        [&](TileDilationPassData& data, RGBuilder& builder)
        {
            builder.SkipCulling();
            data.input = builder.Read(horizontalPass->output);

            RGTexture::Desc desc;
            desc.width = tiledSizeX;
            desc.height = tiledSizeY;
            desc.format = GfxFormat::R16F;

            data.output = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearCoc Tiled"));
        },
        [=](const TileDilationPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
            };

            pCommandList->SetPipelineState(m_pTileCocYPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(tiledSizeX, 8), DivideRoudingUp(tiledSizeY, 8), 1);
        });

    return verticalPass->output;
}

RGHandle DoF::AddNearBlurPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle coc, uint32_t width, uint32_t height)
{
    struct NearBlurXPassData
    {
        RGHandle input;
        RGHandle coc;
        RGHandle outputR;
        RGHandle outputG;
        RGHandle outputB;
        RGHandle outputWeights;
    };

    auto horizontalPass = pRenderGraph->AddPass<NearBlurXPassData>("NearBlur X", RenderPassType::Compute,
        [&](NearBlurXPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.coc = builder.Read(coc);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RG16F;

            data.outputR = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearBlur R"));
            data.outputG = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearBlur G"));
            data.outputB = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearBlur B"));

            desc.format = GfxFormat::R16F;
            data.outputWeights = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearBlur Weights"));
        },
        [=](const NearBlurXPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.coc)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputR)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.outputG)->GetUAV()->GetHeapIndex(),

                pRenderGraph->GetTexture(data.outputB)->GetUAV()->GetHeapIndex(),
                0,
                pRenderGraph->GetTexture(data.outputWeights)->GetUAV()->GetHeapIndex(),
                asuint(m_maxCocSize),
            };

            pCommandList->SetPipelineState(m_pNearHorizontalBlurPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    struct NearBlurYPassData
    {
        RGHandle input;
        RGHandle coc;
        RGHandle R;
        RGHandle G;
        RGHandle B;
        RGHandle weights;
        RGHandle output;
    };

    auto verticalPass = pRenderGraph->AddPass<NearBlurYPassData>("NearBlur Y", RenderPassType::Compute,
        [&](NearBlurYPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.coc = builder.Read(coc);
            data.R = builder.Read(horizontalPass->outputR);
            data.G = builder.Read(horizontalPass->outputG);
            data.B = builder.Read(horizontalPass->outputB);
            data.weights = builder.Read(horizontalPass->outputWeights);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.output = builder.Write(builder.Create<RGTexture>(desc, "DoF - NearBlur"));
        },
        [=](const NearBlurYPassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.input)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.coc)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.R)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.G)->GetSRV()->GetHeapIndex(),

                pRenderGraph->GetTexture(data.B)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.weights)->GetSRV()->GetHeapIndex(),
                asuint(m_maxCocSize),
            };

            pCommandList->SetPipelineState(m_pNearVerticalBlurPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    return verticalPass->output;
}

RGHandle DoF::AddCompositePass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, RGHandle farBlur, RGHandle nearBlur, uint32_t width, uint32_t height)
{
    struct PassData
    {
        RGHandle color;
        RGHandle depth;
        RGHandle farBlur;
        RGHandle nearBlur;
        RGHandle output;
    };

    auto pass = pRenderGraph->AddPass<PassData>("Composite", RenderPassType::Compute,
        [&](PassData& data, RGBuilder& builder)
        {
            data.color = builder.Read(color);
            data.depth = builder.Read(depth);
            data.farBlur = builder.Read(farBlur);
            data.nearBlur = builder.Read(nearBlur);
            
            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;

            data.output = builder.Write(builder.Create<RGTexture>(desc, "DoF ouput"));
        },
        [=](const PassData& data, IGfxCommandList* pCommandList)
        {
            uint32_t cb[] =
            {
                pRenderGraph->GetTexture(data.color)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.depth)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.farBlur)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.nearBlur)->GetSRV()->GetHeapIndex(),
                pRenderGraph->GetTexture(data.output)->GetUAV()->GetHeapIndex(),
                asuint(m_focusDistance),
                asuint(m_maxCocSize),
            };
            
            pCommandList->SetPipelineState(m_pCompositePSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        });

    return pass->output;
}
