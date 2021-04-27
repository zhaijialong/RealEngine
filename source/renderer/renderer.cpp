#include "renderer.h"
#include "core/engine.h"
#include <fstream>

Renderer::Renderer()
{
    m_pShaderCompiler = std::make_unique<ShaderCompiler>();
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

void Renderer::CreateResources()
{
    //test code ...
    std::string source;

    std::string file = Engine::GetInstance()->GetShaderPath() + "mesh.hlsl";
    std::ifstream is;
    is.open(file.c_str(), std::ios::binary);
    if (!is.fail())
    {
        is.seekg(0, std::ios::end);
        uint32_t length = (uint32_t)is.tellg();
        is.seekg(0, std::ios::beg);

        source.resize(length);

        is.read((char*)source.data(), length);
        is.close();
    }

    std::vector<uint8_t> shader_blob;
    m_pShaderCompiler->Compile(source, "mesh.hlsl", "main", "vs_6_6", { "ENABLE_TEST=1" }, shader_blob);
}
