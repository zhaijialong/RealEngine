#include "renderer.h"
#include "core/engine.h"

Renderer::Renderer() : m_resizeConnection({})
{
    m_pShaderCompiler = std::make_unique<ShaderCompiler>();
    m_pShaderCache = std::make_unique<ShaderCache>(this);
    m_pPipelineCache = std::make_unique<PipelineStateCache>(this);

    m_resizeConnection = Engine::GetInstance()->WindowResizeSignal.connect(this, &Renderer::OnWindowResize);
}

Renderer::~Renderer()
{
    WaitGpuFinished();

    Engine::GetInstance()->WindowResizeSignal.disconnect(m_resizeConnection);
}

void Renderer::CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync)
{
    GfxDeviceDesc desc;
    desc.max_frame_lag = MAX_INFLIGHT_FRAMES;
    m_pDevice.reset(CreateGfxDevice(desc));

    GfxSwapchainDesc swapchainDesc;
    swapchainDesc.window_handle = window_handle;
    swapchainDesc.width = window_width;
    swapchainDesc.height = window_height;
    swapchainDesc.enable_vsync = enable_vsync;
    m_pSwapchain.reset(m_pDevice->CreateSwapchain(swapchainDesc, "Renderer::m_pSwapchain"));

    m_pFrameFence.reset(m_pDevice->CreateFence("Renderer::m_pFrameFence"));

    for (int i = 0; i < MAX_INFLIGHT_FRAMES; ++i)
    {
        std::string name = "Renderer::m_pCommandLists[" + std::to_string(i) + "]";
        m_pCommandLists[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Graphics, name));
    }

    CreateCommonResources();
}

void Renderer::RenderFrame()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    m_pFrameFence->Wait(m_nFrameFenceValue[frame_index]);
    m_pDevice->BeginFrame();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Begin();
    pCommandList->BeginEvent("Renderer::RenderFrame");

    IGfxTexture* pBackBuffer = m_pSwapchain->GetBackBuffer();
    pCommandList->ResourceBarrier(pBackBuffer, 0, GfxResourceState::Present, GfxResourceState::RenderTarget);

    GfxRenderPassDesc render_pass;
    render_pass.color[0].texture = pBackBuffer;
    render_pass.color[0].load_op = GfxRenderPassLoadOp::Clear;
    render_pass.color[0].clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    pCommandList->BeginRenderPass(render_pass);

    pCommandList->SetViewport(0, 0, pBackBuffer->GetDesc().width, pBackBuffer->GetDesc().height);

    {
        RENDER_EVENT(pCommandList, "Render something");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = GetShader("mesh.hlsl", "vs_main", "vs_6_6", {});
        psoDesc.ps = GetShader("mesh.hlsl", "ps_main", "ps_6_6", {});
        psoDesc.rt_format[0] = m_pSwapchain->GetBackBuffer()->GetDesc().format;

        IGfxPipelineState* pPSO = GetPipelineState(psoDesc, "test pso");
        pCommandList->SetPipelineState(pPSO);

        Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
        pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 1, (void*)&camera->GetViewProjectionMatrix(), sizeof(float4x4));

        pCommandList->Draw(3, 1);
    }

    pCommandList->EndRenderPass();
    pCommandList->ResourceBarrier(pBackBuffer, 0, GfxResourceState::RenderTarget, GfxResourceState::Present);

    pCommandList->EndEvent();
    pCommandList->End();

    ++m_nCurrentFenceValue;
    m_nFrameFenceValue[frame_index] = m_nCurrentFenceValue;

    pCommandList->Submit();
    m_pSwapchain->Present();
    pCommandList->Signal(m_pFrameFence.get(), m_nCurrentFenceValue);

    m_pDevice->EndFrame();
}

void Renderer::WaitGpuFinished()
{
    m_pFrameFence->Wait(m_nCurrentFenceValue);
}

IGfxShader* Renderer::GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    return m_pShaderCache->GetShader(file, entry_point, profile, defines);
}

IGfxPipelineState* Renderer::GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

void Renderer::CreateCommonResources()
{
    GfxSamplerDesc desc;
    m_nPointSampler = m_pDevice->CreateSampler(desc);

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    m_nLinearSampler = m_pDevice->CreateSampler(desc);
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height)
{
    WaitGpuFinished();

    m_pSwapchain->Resize(width, height);
}
