#include "xess.h"

#if RE_PLATFORM_WINDOWS

#include "../renderer.h"
#include "utils/gui_util.h"
#include "utils/log.h"
#include "xess/xess_d3d12.h"

#pragma comment(lib, "libxess.lib")

static void XeSSLog(const char* message, xess_logging_level_t logging_level)
{
    switch (logging_level)
    {
    case XESS_LOGGING_LEVEL_DEBUG:
        RE_DEBUG(message);
        break;
    case XESS_LOGGING_LEVEL_INFO:
        RE_INFO(message);
        break;
    case XESS_LOGGING_LEVEL_WARNING:
        RE_WARN(message);
        break;
    case XESS_LOGGING_LEVEL_ERROR:
        RE_ERROR(message);
        break;
    default:
        break;
    }
}

XeSS::XeSS(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    IGfxDevice* pDevice = m_pRenderer->GetDevice();
    if (pDevice->GetDesc().backend == GfxRenderBackend::D3D12)
    {
        ID3D12Device* device = (ID3D12Device*)m_pRenderer->GetDevice()->GetHandle();
        xess_result_t result = xessD3D12CreateContext(device, &m_context);
        RE_ASSERT(result == XESS_RESULT_SUCCESS);

        xessSetLoggingCallback(m_context, XESS_LOGGING_LEVEL_DEBUG, XeSSLog);
    }

    Engine::GetInstance()->WindowResizeSignal.connect(&XeSS::OnWindowResize, this);
}

XeSS::~XeSS()
{
    if (m_context)
    {
        xessDestroyContext(m_context);
    }
}

RGHandle XeSS::AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    xess_version_t version;
    xessGetVersion(&version);

    GUI("PostProcess", fmt::format("XeSS {}.{}.{}", version.major, version.minor, version.patch).c_str(), 
        [&, displayWidth, displayHeight]()
        {
            TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
            if (mode == TemporalSuperResolution::XeSS)
            {
                int qualityMode = m_quality - XESS_QUALITY_SETTING_PERFORMANCE;
                if (ImGui::Combo("Mode##XeSS", (int*)&qualityMode, "Performance (2.0x)\0Balanced (1.7x)\0Quality (1.5x)\0Ultra Quality (1.3x)\0\0", 4))
                {
                    m_quality = (xess_quality_settings_t)(qualityMode + XESS_QUALITY_SETTING_PERFORMANCE);
                    m_needInitialization = true;
                }

                m_pRenderer->SetTemporalUpscaleRatio(GetUpscaleRatio(displayWidth, displayHeight));
            }
        });

    struct XeSSData
    {
        RGHandle input;
        RGHandle depth;
        RGHandle velocity;
        RGHandle exposure;
        RGHandle output;
    };

    auto xess_pass = pRenderGraph->AddPass<XeSSData>("XeSS", RenderPassType::Compute,
        [&](XeSSData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.exposure = builder.Read(exposure);

            RGTexture::Desc desc;
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "XeSS Output");
            data.output = builder.Write(data.output);
        },
        [=](const XeSSData& data, IGfxCommandList* pCommandList)
        {
            InitializeXeSS(displayWidth, displayHeight);

            pCommandList->FlushBarriers();

            RGTexture* inputRT = pRenderGraph->GetTexture(data.input);
            RGTexture* depthRT = pRenderGraph->GetTexture(data.depth);
            RGTexture* velocityRT = pRenderGraph->GetTexture(data.velocity);
            RGTexture* exposureRT = pRenderGraph->GetTexture(data.exposure);
            RGTexture* outputRT = pRenderGraph->GetTexture(data.output);

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            xess_d3d12_execute_params_t params = {};
            params.pColorTexture = (ID3D12Resource*)inputRT->GetTexture()->GetHandle();
            params.pVelocityTexture = (ID3D12Resource*)velocityRT->GetTexture()->GetHandle();
            params.pDepthTexture = (ID3D12Resource*)depthRT->GetTexture()->GetHandle();
            params.pExposureScaleTexture = (ID3D12Resource*)exposureRT->GetTexture()->GetHandle();
            params.pOutputTexture = (ID3D12Resource*)outputRT->GetTexture()->GetHandle();
            params.jitterOffsetX = camera->GetJitter().x;
            params.jitterOffsetY = camera->GetJitter().y;
            params.exposureScale = 1.0f;
            params.resetHistory = false;
            params.inputWidth = renderWidth;
            params.inputHeight = renderHeight;

            xessSetJitterScale(m_context, 1.0f, 1.0f);
            xessSetVelocityScale(m_context, -0.5f * renderWidth, 0.5f * renderHeight);

            xess_result_t result = xessD3D12Execute(m_context, (ID3D12GraphicsCommandList*)pCommandList->GetHandle(), &params);
            RE_ASSERT(result == XESS_RESULT_SUCCESS);

            pCommandList->ResetState();
            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    return xess_pass->output;
}

float XeSS::GetUpscaleRatio(uint32_t displayWidth, uint32_t displayHeight) const
{
    xess_2d_t outputResolution = { displayWidth, displayHeight };
    xess_2d_t inputResolution;
    xessGetInputResolution(m_context, &outputResolution, m_quality, &inputResolution);
    return (float)outputResolution.x / (float)inputResolution.x;
}

void XeSS::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    m_needInitialization = true;
}

void XeSS::InitializeXeSS(uint32_t displayWidth, uint32_t displayHeight)
{
    if (m_needInitialization)
    {
        m_pRenderer->WaitGpuFinished();

        xess_d3d12_init_params_t params = {};
        params.outputResolution.x = displayWidth;
        params.outputResolution.y = displayHeight;
        params.qualitySetting = m_quality;
        params.initFlags = XESS_INIT_FLAG_INVERTED_DEPTH | 
            XESS_INIT_FLAG_EXPOSURE_SCALE_TEXTURE;

        xess_result_t result = xessD3D12Init(m_context, &params);
        RE_ASSERT(result == XESS_RESULT_SUCCESS);

        m_needInitialization = false;
    }
}

#endif // #if RE_PLATFORM_WINDOWS
