#include "automatic_exposure.h"
#include "../renderer.h"
#include "utils/gui_util.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

AutomaticExposure::AutomaticExposure(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("automatic_exposure.hlsl", "luminance_reduction", "cs_6_6", {});
    m_pLuminanceReductionPSO = pRenderer->GetPipelineState(desc, "Luminance Reduction PSO");

    m_pSPDCounterBuffer.reset(pRenderer->CreateTypedBuffer(nullptr, GfxFormat::R32UI, 1, "AutomaticExposure::m_pSPDCounterBuffer", GfxMemoryType::GpuOnly, true));
    m_pPreviousEV100.reset(pRenderer->CreateTexture2D(1, 1, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "AutomaticExposure::m_pPreviousEV100"));
}

RenderGraphHandle AutomaticExposure::Render(RenderGraph* pRenderGraph, RenderGraphHandle sceneColorRT, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "AutomaticExposure",
        [&]()
        {
            ImGui::Combo("Exposure Mode##Exposure", (int*)&m_exposuremode, "Automatic\0AutomaticHistogram\0Manual\0\0");
            ImGui::Combo("Metering Mode##Exposure", (int*)&m_meteringMode, "Average\0Spot\0CenterWeighted\00");
            ImGui::SliderFloat("Min Luminance##Exposure", &m_minLuminance, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Max Luminance##Exposure", &m_maxLuminance, 0.3f, 10.0f, "%.2f");
            ImGui::SliderFloat("Eye Adaption Speed##Exposure", &m_adaptionSpeed, 0.01, 5.0f, "%.2f");
            ImGui::Checkbox("Debug EV100##Exposure", &m_bDebugEV100);
        });

    RENDER_GRAPH_EVENT(pRenderGraph, "AutomaticExposure");

    RenderGraphHandle avgLuminanceRT;

    if (m_exposuremode == ExposureMode::Automatic)
    {
        ComputeLuminanceSize(width, height);

        struct InitLuminanceData
        {
            RenderGraphHandle input;
            RenderGraphHandle output;
        };

        RenderGraphHandle luminanceRT;

        auto init_luminance_pass = pRenderGraph->AddPass<InitLuminanceData>("Init Luminance",
            [&](InitLuminanceData& data, RenderGraphBuilder& builder)
            {
                data.input = builder.Read(sceneColorRT, GfxResourceState::ShaderResourceNonPS);

                RenderGraphTexture::Desc desc;
                desc.width = m_luminanceSize.x;
                desc.height = m_luminanceSize.y;
                desc.mip_levels = m_luminanceMips;
                desc.format = GfxFormat::RG16F;
                desc.usage = GfxTextureUsageUnorderedAccess;

                luminanceRT = builder.Create<RenderGraphTexture>(desc, "Average Luminance");
                data.output = builder.Write(luminanceRT, GfxResourceState::UnorderedAccess, 0);
            },
            [=](const InitLuminanceData& data, IGfxCommandList* pCommandList)
            {
                RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
                RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
                InitLuminance(pCommandList, input->GetSRV(), output->GetUAV());
            });

        struct LuminanceReductionData
        {
            RenderGraphHandle luminanceRT;
        };

        auto luminance_reduction_pass = pRenderGraph->AddPass<LuminanceReductionData>("Luminance Reduction",
            [&](LuminanceReductionData& data, RenderGraphBuilder& builder)
            {
                data.luminanceRT = builder.Read(init_luminance_pass->output, GfxResourceState::ShaderResourceNonPS, 0);

                for (uint32_t i = 1; i < m_luminanceMips; ++i)
                {
                    data.luminanceRT = builder.Write(luminanceRT, GfxResourceState::UnorderedAccess, i);
                }
            },
            [=](const LuminanceReductionData& data, IGfxCommandList* pCommandList)
            {
                RenderGraphTexture* texture = (RenderGraphTexture*)pRenderGraph->GetResource(data.luminanceRT);
                ReduceLuminance(pCommandList, texture);
            });

        avgLuminanceRT = luminance_reduction_pass->luminanceRT;
    }
    else if (m_exposuremode == ExposureMode::AutomaticHistogram)
    {
        //todo
    }

    struct ExposureData
    {
        RenderGraphHandle avgLuminance;
        RenderGraphHandle exposure;
    };

    auto exposure_pass = pRenderGraph->AddPass<ExposureData>("Exposure",
        [&](ExposureData& data, RenderGraphBuilder& builder)
        {
            if (avgLuminanceRT.IsValid())
            {
                data.avgLuminance = builder.Read(avgLuminanceRT, GfxResourceState::ShaderResourceNonPS, m_luminanceMips - 1);
            }

            RenderGraphTexture::Desc desc;
            desc.width = desc.height = 1;
            desc.format = GfxFormat::R16F;
            desc.usage = GfxTextureUsageUnorderedAccess;
            data.exposure = builder.Create<RenderGraphTexture>(desc, "Exposure");
            data.exposure = builder.Write(data.exposure, GfxResourceState::UnorderedAccess);
        },
        [=](const ExposureData& data, IGfxCommandList* pCommandList)
        {
            IGfxDescriptor* avgLuminanceSRV = nullptr;
            if (data.avgLuminance.IsValid())
            {
                RenderGraphTexture* avgLuminance = (RenderGraphTexture*)pRenderGraph->GetResource(data.avgLuminance);
                avgLuminanceSRV = avgLuminance->GetSRV();
            }

            RenderGraphTexture* exposure = (RenderGraphTexture*)pRenderGraph->GetResource(data.exposure);
            Exposure(pCommandList, avgLuminanceSRV, exposure->GetUAV());
        });

    return exposure_pass->exposure;
}

void AutomaticExposure::ComputeLuminanceSize(uint32_t width, uint32_t height)
{
    uint32_t mipsX = (uint32_t)std::max(ceilf(log2f((float)width)), 1.0f);
    uint32_t mipsY = (uint32_t)std::max(ceilf(log2f((float)height)), 1.0f);

    m_luminanceMips = std::max(mipsX, mipsY);
    RE_ASSERT(m_luminanceMips <= 13); //spd limit

    m_luminanceSize.x = 1 << (mipsX - 1);
    m_luminanceSize.y = 1 << (mipsY - 1);
}

void AutomaticExposure::InitLuminance(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output)
{
    std::vector<std::string> defines;

    switch (m_meteringMode)
    {
    case MeteringMode::Average:
        defines.push_back("METERING_MODE_AVERAGE=1");
        break;
    case MeteringMode::Spot:
        defines.push_back("METERING_MODE_SPOT=1");
        break;
    case MeteringMode::CenterWeighted:
        defines.push_back("METERING_MODE_CENTER_WEIGHTED=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    GfxComputePipelineDesc desc;
    desc.cs = m_pRenderer->GetShader("automatic_exposure.hlsl", "init_luminance", "cs_6_6", defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(desc, "Init Luminance PSO");

    pCommandList->SetPipelineState(pso);

    struct Constants
    {
        uint input;
        uint output;
        float width;
        float height;
        float rcpWidth;
        float rcpHeight;
        float minLuminance;
        float maxLuminance;
    };

    Constants root_constants = { 
        input->GetHeapIndex(), output->GetHeapIndex(), 
        (float)m_luminanceSize.x, (float)m_luminanceSize.y,
        1.0f / m_luminanceSize.x, 1.0f / m_luminanceSize.y, 
        m_minLuminance, m_maxLuminance };
    pCommandList->SetComputeConstants(1, &root_constants, sizeof(root_constants));

    pCommandList->Dispatch((m_luminanceSize.x + 7) / 8, (m_luminanceSize.y + 7) / 8, 1);
}

void AutomaticExposure::ReduceLuminance(IGfxCommandList* pCommandList, RenderGraphTexture* texture)
{
    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopyDst);
    pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);
    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);

    pCommandList->SetPipelineState(m_pLuminanceReductionPSO);

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, m_luminanceSize.x, m_luminanceSize.y); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, m_luminanceMips - 1);

    struct spdConstants
    {
        uint mips;
        uint numWorkGroups;
        uint2 workGroupOffset;

        float2 invInputSize;
        uint c_imgSrc;
        uint c_spdGlobalAtomicUAV;

        //hlsl packing rules : every element in an array is stored in a four-component vector
        uint4 c_imgDst[12]; // do no access MIP [5]
    };

    spdConstants constants = {};
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.mips = numWorkGroupsAndMips[1];
    constants.workGroupOffset[0] = workGroupOffset[0];
    constants.workGroupOffset[1] = workGroupOffset[1];
    constants.invInputSize[0] = 1.0f / m_luminanceSize.x;
    constants.invInputSize[1] = 1.0f / m_luminanceSize.y;

    constants.c_imgSrc = texture->GetSRV()->GetHeapIndex();
    constants.c_spdGlobalAtomicUAV = m_pSPDCounterBuffer->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < m_luminanceMips - 1; ++i)
    {
        constants.c_imgDst[i].x = texture->GetUAV(i + 1, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 1; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    for (uint32_t i = 1; i < m_luminanceMips - 1; ++i)
    {
        //todo : currently RG doesn't hanlde subresource last usage properly, this fixed validation warning
        pCommandList->ResourceBarrier(texture->GetTexture(), i, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceNonPS);
    }
}

void AutomaticExposure::Exposure(IGfxCommandList* pCommandList, IGfxDescriptor* avgLuminance, IGfxDescriptor* output)
{
    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;

        float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pPreviousEV100->GetTexture(), m_pPreviousEV100->GetUAV(), clear_value);
        pCommandList->UavBarrier(m_pPreviousEV100->GetTexture());
    }

    std::vector<std::string> defines;

    switch (m_exposuremode)
    {
    case ExposureMode::Automatic:
        defines.push_back("EXPOSURE_MODE_AUTO=1");
        break;
    case ExposureMode::AutomaticHistogram:
        defines.push_back("EXPOSURE_MODE_AUTO_HISTOGRAM=1");
        break;
    case ExposureMode::Manual:
        defines.push_back("EXPOSURE_MODE_MANUAL=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    if (m_bDebugEV100)
    {
        defines.push_back("DEBUG_SHOW_EV100=1");
    }

    GfxComputePipelineDesc desc;
    desc.cs = m_pRenderer->GetShader("automatic_exposure.hlsl", "exposure", "cs_6_6", defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(desc, "Exposure PSO");

    pCommandList->SetPipelineState(pso);

    struct ExpsureConstants
    {
        uint avgLuminanceTexture;
        uint avgLuminanceMip;
        uint exposureTexture;
        uint previousEV100Texture;
        float adaptionSpeed;
    };

    ExpsureConstants root_constants = {
        avgLuminance ? avgLuminance->GetHeapIndex() : GFX_INVALID_RESOURCE,
        m_luminanceMips - 1, 
        output->GetHeapIndex(),
        m_pPreviousEV100->GetUAV()->GetHeapIndex(),
        m_adaptionSpeed
    };

    pCommandList->SetComputeConstants(0, &root_constants, sizeof(root_constants));
    pCommandList->Dispatch(1, 1, 1);
}
