#include "renderer.h"
#include "core/engine.h"

Renderer::Renderer()
{
    m_pShaderCompiler = std::make_unique<ShaderCompiler>();
    m_pShaderCache = std::make_unique<ShaderCache>(this);
    m_pPipelineCache = std::make_unique<PipelineStateCache>(this);
}

Renderer::~Renderer()
{
    m_pFrameFence->Wait(m_nCurrentFenceValue);
}

void Renderer::CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height)
{
    GfxDeviceDesc desc;
    desc.max_frame_lag = MAX_INFLIGHT_FRAMES;
    m_pDevice.reset(CreateGfxDevice(desc));

    GfxSwapchainDesc swapchainDesc;
    swapchainDesc.windowHandle = window_handle;
    swapchainDesc.width = window_width;
    swapchainDesc.height = window_height;
    m_pSwapchain.reset(m_pDevice->CreateSwapchain(swapchainDesc, "Renderer::m_pSwapchain"));

    m_pFrameFence.reset(m_pDevice->CreateFence("Renderer::m_pFrameFence"));

    for (int i = 0; i < MAX_INFLIGHT_FRAMES; ++i)
    {
        std::string name = "Renderer::m_pCommandLists " + std::to_string(i);
        m_pCommandLists[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Graphics, name));
    }

    CreateResources();
}

void Renderer::RenderFrame()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    m_pFrameFence->Wait(m_nFrameFenceValue[frame_index]);
    m_pDevice->BeginFrame();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Begin();

    IGfxTexture* pBackBuffer = m_pSwapchain->GetBackBuffer();
    pCommandList->ResourceBarrier(pBackBuffer, 0, GfxResourceState::Present, GfxResourceState::RenderTarget);

    GfxRenderPassDesc render_pass;
    render_pass.color[0].texture = pBackBuffer;
    render_pass.color[0].load_op = GfxRenderPassLoadOp::Clear;
    render_pass.color[0].clear_color = { 1.0f, 1.0f, 0.0f, 1.0f };
    pCommandList->BeginRenderPass(render_pass);

    pCommandList->SetViewport(0, 0, pBackBuffer->GetDesc().width, pBackBuffer->GetDesc().height);

    //todo : render something

    pCommandList->EndRenderPass();
    pCommandList->ResourceBarrier(pBackBuffer, 0, GfxResourceState::RenderTarget, GfxResourceState::Present);
    pCommandList->End();

    ++m_nCurrentFenceValue;
    m_nFrameFenceValue[frame_index] = m_nCurrentFenceValue;

    pCommandList->Signal(m_pFrameFence.get(), m_nCurrentFenceValue);
    pCommandList->Submit();

    m_pSwapchain->Present();

    m_pDevice->EndFrame();
}

IGfxShader* Renderer::GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    return m_pShaderCache->GetShader(file, entry_point, profile, defines);
}

void Renderer::CreateResources()
{
    //test code ...
    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = GetShader("mesh.hlsl", "vs_main", "vs_6_6", { "TEST=1", "FOOBAR=1" });
    psoDesc.ps = GetShader("mesh.hlsl", "ps_main", "ps_6_6", { "TEST=1", "FOOBAR=1" });
    psoDesc.rt_format[0] = m_pSwapchain->GetBackBuffer()->GetDesc().format;

    IGfxPipelineState* pPSO = m_pDevice->CreateGraphicsPipelineState(psoDesc, "test pso");
}
