#include "dlss.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_helpers.h"

#ifdef _DEBUG
#pragma comment(lib, "external/DLSS/lib/Windows_x86_64/x86_64/nvsdk_ngx_d_dbg.lib")
#else
#pragma comment(lib, "external/DLSS/lib/Windows_x86_64/x86_64/nvsdk_ngx_d.lib")
#endif

#define APP_ID 231313132

DLSS::DLSS(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_dlssAvailable = InitializeNGX();

    Engine::GetInstance()->WindowResizeSignal.connect(&DLSS::OnWindowResize, this);
}

DLSS::~DLSS()
{
    ReleaseDLSSFeatures();
    ShutdownNGX();
}

RenderGraphHandle DLSS::Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depth, RenderGraphHandle velocity, 
    RenderGraphHandle exposure, uint32_t displayWidth, uint32_t displayHeight)
{
    if (!IsSupported())
    {
        return input;
    }

    GUI("PostProcess", "DLSS", [&]()
        {
            TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
            if (mode == TemporalSuperResolution::DLSS)
            {
                //todo : Ultra Performance crashes
                ImGui::Combo("Mode##DLSS", &m_qualityMode, "Performance (2.0x)\0Balanced (1.7x)\0Quality (1.5x)\0Ultra Performance (3.0x)\0Custom\0", 5);

                if (m_qualityMode == 4)
                {
                    ImGui::SliderFloat("Upscale Ratio##DLSS", &m_customUpscaleRatio, 1.0, 2.0); //todo : 3.0 crashes
                }

                ImGui::SliderFloat("Sharpness##DLSS", &m_sharpness, 0.0f, 1.0f, "%.2f");

                m_pRenderer->SetTemporalUpscaleRatio(GetUpscaleRatio());
            }
        });

    struct DLSSPassData
    {
        RenderGraphHandle input;
        RenderGraphHandle depth;
        RenderGraphHandle velocity;
        RenderGraphHandle exposure;
        RenderGraphHandle output;
    };

    auto dlss_pass = pRenderGraph->AddPass<DLSSPassData>("DLSS", RenderPassType::Compute,
        [&](DLSSPassData& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(input);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.exposure = builder.Read(exposure);

            RenderGraphTexture::Desc desc;
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "DLSS Output");
            data.output = builder.Write(data.output);
        },
        [=](const DLSSPassData& data, IGfxCommandList* pCommandList)
        {
            if (m_needInitializeDlss)
            {
                ReleaseDLSSFeatures();
                InitializeDLSSFeatures(pCommandList);
            }

            pCommandList->FlushBarriers();

            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* exposureRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.exposure);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            NVSDK_NGX_D3D12_DLSS_Eval_Params dlssEvalParams = {};
            dlssEvalParams.Feature.pInColor = (ID3D12Resource*)inputRT->GetTexture()->GetHandle();
            dlssEvalParams.Feature.pInOutput = (ID3D12Resource*)outputRT->GetTexture()->GetHandle();
            dlssEvalParams.pInDepth = (ID3D12Resource*)depthRT->GetTexture()->GetHandle();
            dlssEvalParams.pInMotionVectors = (ID3D12Resource*)velocityRT->GetTexture()->GetHandle();
            dlssEvalParams.pInExposureTexture = (ID3D12Resource*)exposureRT->GetTexture()->GetHandle();
            dlssEvalParams.InJitterOffsetX = camera->GetJitter().x;
            dlssEvalParams.InJitterOffsetY = camera->GetJitter().y;
            dlssEvalParams.Feature.InSharpness = m_sharpness;
            dlssEvalParams.InReset = false;
            dlssEvalParams.InMVScaleX = -0.5f * (float)m_pRenderer->GetRenderWidth();
            dlssEvalParams.InMVScaleY = 0.5f * (float)m_pRenderer->GetRenderHeight();
            dlssEvalParams.InRenderSubrectDimensions = { m_pRenderer->GetRenderWidth(), m_pRenderer->GetRenderHeight() };

            NVSDK_NGX_Result result = NGX_D3D12_EVALUATE_DLSS_EXT((ID3D12GraphicsCommandList*)pCommandList->GetHandle(), m_dlssFeature, m_ngxParameters, &dlssEvalParams);
            RE_ASSERT(NVSDK_NGX_SUCCEED(result));

            pCommandList->ClearState();
            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    return dlss_pass->output;
}

float DLSS::GetUpscaleRatio() const
{
    if (m_qualityMode == 4)
    {
        return m_customUpscaleRatio;
    }

    unsigned int optimalWidth;
    unsigned int optimalHeight;
    unsigned int maxWidth;
    unsigned int maxHeight;
    unsigned int minWidth;
    unsigned int minHeight;
    float sharpness;
    NGX_DLSS_GET_OPTIMAL_SETTINGS(m_ngxParameters, m_pRenderer->GetDisplayWidth(), m_pRenderer->GetDisplayHeight(),
        (NVSDK_NGX_PerfQuality_Value)m_qualityMode,
        &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);

    return (float)m_pRenderer->GetDisplayWidth() / (float)optimalWidth;
}

void DLSS::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    m_needInitializeDlss = true;
}

bool DLSS::InitializeNGX()
{
    if (m_pRenderer->GetDevice()->GetVendor() != GfxVendor::Nvidia)
    {
        return false;
    }

    ID3D12Device* device = (ID3D12Device*)m_pRenderer->GetDevice()->GetHandle();
    NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Init(APP_ID, L".", device); // this throws std::runtime_error ...
    if(NVSDK_NGX_FAILED(result))
    {
        return false;
    }

    result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngxParameters);
    if (NVSDK_NGX_FAILED(result))
    {
        return false;
    }

    int dlssAvailable = 0;
    result = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
    if (NVSDK_NGX_FAILED(result) || !dlssAvailable)
    {
        return false;
    }

    return true;
}

void DLSS::ShutdownNGX()
{
    if (m_ngxParameters)
    {
        NVSDK_NGX_D3D12_DestroyParameters(m_ngxParameters);
    }

    NVSDK_NGX_D3D12_Shutdown(); //but still lots of "D3D12 WARNING: Live Object" ...
}

bool DLSS::InitializeDLSSFeatures(IGfxCommandList* pCommandList)
{
    if (!m_dlssAvailable)
    {
        return false;
    }

    if (m_needInitializeDlss)
    {
        uint32_t displayWidth = m_pRenderer->GetDisplayWidth();
        uint32_t displayHeight = m_pRenderer->GetDisplayHeight();

        unsigned int optimalWidth;
        unsigned int optimalHeight;
        unsigned int maxWidth;
        unsigned int maxHeight;
        unsigned int minWidth;
        unsigned int minHeight;
        float sharpness;
        NGX_DLSS_GET_OPTIMAL_SETTINGS(m_ngxParameters, displayWidth, displayHeight, (NVSDK_NGX_PerfQuality_Value)m_qualityMode,
            &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);

        NVSDK_NGX_DLSS_Create_Params dlssCreateParams = {};
        dlssCreateParams.Feature.InWidth = displayWidth; // optimalWidth;
        dlssCreateParams.Feature.InHeight = displayHeight;// optimalHeight;
        dlssCreateParams.Feature.InTargetWidth = displayWidth;
        dlssCreateParams.Feature.InTargetHeight = displayHeight;
        dlssCreateParams.Feature.InPerfQualityValue = (NVSDK_NGX_PerfQuality_Value)m_qualityMode;
        dlssCreateParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
            NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
            NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
            NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;

        ID3D12GraphicsCommandList* d3d12CommandList = (ID3D12GraphicsCommandList*)pCommandList->GetHandle();
        //"D3D12 WARNING: ID3D12Device::CreateCommittedResource: Ignoring InitialState D3D12_RESOURCE_STATE_COPY_DEST. Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON"
        NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSS_EXT(d3d12CommandList, 0, 0, &m_dlssFeature, m_ngxParameters, &dlssCreateParams);
        RE_ASSERT(NVSDK_NGX_SUCCEED(result));

        pCommandList->UavBarrier(nullptr);

        m_needInitializeDlss = false;
    }

    return true;
}

void DLSS::ReleaseDLSSFeatures()
{
    if (m_dlssFeature)
    {
        m_pRenderer->WaitGpuFinished();

        NVSDK_NGX_D3D12_ReleaseFeature(m_dlssFeature);
        m_dlssFeature = nullptr;
    }
}
