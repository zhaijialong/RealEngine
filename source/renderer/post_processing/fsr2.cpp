#include "fsr2.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "FSR2/dx12/ffx_fsr2_dx12.h"

FSR2::FSR2(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    CreateFsr2Context(pRenderer->GetDisplayWidth(), pRenderer->GetDisplayHeight());

    Engine::GetInstance()->WindowResizeSignal.connect(&FSR2::OnWindowResize, this);
}

FSR2::~FSR2()
{
    DestroyFsr2Context();
}

RenderGraphHandle FSR2::Render(RenderGraph* pRenderGraph, RenderGraphHandle input, RenderGraphHandle depth, RenderGraphHandle velocity, 
    uint32_t displayWidth, uint32_t displayHeight)
{
    GUI("PostProcess", "FSR 2.0", [&]()
        {
            ImGui::SliderFloat("Sharpness##CAS", &m_sharpness, 0.0f, 1.0f, "%.2f");
        });

    struct FSR2Data
    {
        RenderGraphHandle input;
        RenderGraphHandle depth;
        RenderGraphHandle velocity;
        RenderGraphHandle output;
    };

    auto fsr2_pass = pRenderGraph->AddPass<FSR2Data>("FSR 2.0", RenderPassType::Compute,
        [&](FSR2Data& data, RenderGraphBuilder& builder)
        {
            data.input = builder.Read(input);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);

            RenderGraphTexture::Desc desc;
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RenderGraphTexture>(desc, "FSR2 Output");
            data.output = builder.Write(data.output);
        },
        [=](const FSR2Data& data, IGfxCommandList* pCommandList)
        {
            pCommandList->FlushBarriers();

            RenderGraphTexture* inputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.depth);
            RenderGraphTexture* velocityRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.velocity);
            RenderGraphTexture* outputRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            FfxFsr2DispatchDescription dispatchDesc = {};
            dispatchDesc.commandList = ffxGetCommandListDX12((ID3D12CommandList*)pCommandList->GetHandle());
            dispatchDesc.color = ffxGetResourceDX12(&m_context, (ID3D12Resource*)inputRT->GetTexture()->GetHandle(), L"FSR2_InputColor");
            dispatchDesc.depth = ffxGetResourceDX12(&m_context, (ID3D12Resource*)depthRT->GetTexture()->GetHandle(), L"FSR2_InputDepth");
            dispatchDesc.motionVectors = ffxGetResourceDX12(&m_context, (ID3D12Resource*)velocityRT->GetTexture()->GetHandle(), L"FSR2_InputMotionVectors");
            dispatchDesc.exposure = ffxGetResourceDX12(&m_context, nullptr, L"FSR2_InputExposure");
            dispatchDesc.reactive = ffxGetResourceDX12(&m_context, nullptr, L"FSR2_InputReactiveMap");
            dispatchDesc.transparencyAndComposition = ffxGetResourceDX12(&m_context, nullptr, L"FSR2_TransparencyAndCompositionMap");
            dispatchDesc.output = ffxGetResourceDX12(&m_context, (ID3D12Resource*)outputRT->GetTexture()->GetHandle(), L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
            dispatchDesc.jitterOffset.x = camera->GetJitter().x;
            dispatchDesc.jitterOffset.y = camera->GetJitter().y;
            dispatchDesc.motionVectorScale.x = -0.5f * (float)m_pRenderer->GetRenderWidth();
            dispatchDesc.motionVectorScale.y = 0.5f * (float)m_pRenderer->GetRenderHeight();
            dispatchDesc.reset = false;
            dispatchDesc.enableSharpening = true;
            dispatchDesc.sharpness = m_sharpness;
            dispatchDesc.frameTimeDelta = Engine::GetInstance()->GetFrameDeltaTime() * 1000.0f;
            dispatchDesc.preExposure = 1.0f;
            dispatchDesc.renderSize.width = m_pRenderer->GetRenderWidth();
            dispatchDesc.renderSize.height = m_pRenderer->GetRenderHeight();
            dispatchDesc.cameraFar = camera->GetZFar();
            dispatchDesc.cameraNear = camera->GetZNear();
            dispatchDesc.cameraFovAngleVertical = degree_to_randian(camera->GetFov());

            FfxErrorCode errorCode = ffxFsr2ContextDispatch(&m_context, &dispatchDesc);
            FFX_ASSERT(errorCode == FFX_OK);

            pCommandList->ClearState();
            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    return fsr2_pass->output;
}

void FSR2::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    if (m_desc.displaySize.width != width ||
        m_desc.displaySize.height != height)
    {
        DestroyFsr2Context();
        CreateFsr2Context(width, height);
    }
}

void FSR2::CreateFsr2Context(uint32_t displayWidth, uint32_t displayHeight)
{
    ID3D12Device* device = (ID3D12Device*)m_pRenderer->GetDevice()->GetHandle();

    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX12();
    void* scratchBuffer = RE_ALLOC(scratchBufferSize);
    FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&m_desc.callbacks, device, scratchBuffer, scratchBufferSize);
    FFX_ASSERT(errorCode == FFX_OK);

    m_desc.device = ffxGetDeviceDX12(device);
    m_desc.maxRenderSize.width = displayWidth; //todo : it can be set to renderSize which decreases memory usage
    m_desc.maxRenderSize.height = displayHeight;
    m_desc.displaySize.width = displayWidth;
    m_desc.displaySize.height = displayHeight;
    m_desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_DEPTH_INVERTED | FFX_FSR2_ENABLE_AUTO_EXPOSURE;

    ffxFsr2ContextCreate(&m_context, &m_desc);
}

void FSR2::DestroyFsr2Context()
{
    ffxFsr2ContextDestroy(&m_context);
    RE_FREE(m_desc.callbacks.scratchBuffer);
}
