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

    m_pUploadFence.reset(m_pDevice->CreateFence("Renderer::m_pUploadFence"));

    for (int i = 0; i < MAX_INFLIGHT_FRAMES; ++i)
    {
        std::string name = "Renderer::m_pUploadCommandList[" + std::to_string(i) + "]";
        m_pUploadCommandList[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Copy, name));

        m_pStagingBufferAllocator[i] = std::make_unique<StagingBufferAllocator>(this);
    }

    CreateCommonResources();
}

void Renderer::RenderFrame()
{
    BeginFrame();
    UploadResources();
    Render();
    EndFrame();
}

void Renderer::BeginFrame()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    m_pFrameFence->Wait(m_nFrameFenceValue[frame_index]);
    m_pDevice->BeginFrame();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Begin();
}

void Renderer::UploadResources()
{
    if (m_pendingTextureUploads.empty() && m_pendingBufferUpload.empty())
    {
        return;
    }

    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    IGfxCommandList* pUploadCommandList = m_pUploadCommandList[frame_index].get();
    pUploadCommandList->Begin();

    {
        RENDER_EVENT(pUploadCommandList, "Renderer::UploadResources");

        for (size_t i = 0; i < m_pendingTextureUploads.size(); ++i)
        {
            const TextureUpload& upload = m_pendingTextureUploads[i];
            pUploadCommandList->CopyBufferToTexture(upload.texture, upload.mip_level, upload.array_slice, 
                upload.staging_buffer.buffer, upload.staging_buffer.offset + upload.offset);
        }
        m_pendingTextureUploads.clear();

        for (size_t i = 0; i < m_pendingBufferUpload.size(); ++i)
        {
            const BufferUpload& upload = m_pendingBufferUpload[i];
            pUploadCommandList->CopyBuffer(upload.buffer, 0,
                upload.staging_buffer.buffer, upload.staging_buffer.offset, upload.staging_buffer.size);
        }
        m_pendingBufferUpload.clear();
    }

    pUploadCommandList->End();
    pUploadCommandList->Submit();

    m_nCurrentUploadFenceValue++;
    pUploadCommandList->Signal(m_pUploadFence.get(), m_nCurrentUploadFenceValue);

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Wait(m_pUploadFence.get(), m_nCurrentUploadFenceValue);
}

void Renderer::Render()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    std::string event_name = "Render Frame " + std::to_string(m_pDevice->GetFrameID());
    RENDER_EVENT(pCommandList, event_name.c_str());

    GfxRenderPassDesc render_pass;
    render_pass.color[0].texture = m_pHdrRT->GetTexture();
    render_pass.color[0].load_op = GfxRenderPassLoadOp::Clear;
    render_pass.color[0].clear_color[0] = 0.0f;
    render_pass.color[0].clear_color[1] = 0.0f;
    render_pass.color[0].clear_color[2] = 0.0f;
    render_pass.color[0].clear_color[3] = 1.0f;
    render_pass.depth.texture = m_pDepthRT->GetTexture();
    render_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
    render_pass.depth.stencil_load_op = GfxRenderPassLoadOp::Clear;

    {
        RENDER_EVENT(pCommandList, "BassPass");

        pCommandList->BeginRenderPass(render_pass);
        pCommandList->SetViewport(0, 0, m_pHdrRT->GetTexture()->GetDesc().width, m_pHdrRT->GetTexture()->GetDesc().height);

        for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
        {
            m_basePassBatchs[i](pCommandList, this, camera);
        }
        m_basePassBatchs.clear();

        pCommandList->EndRenderPass();
    }

    pCommandList->ResourceBarrier(m_pHdrRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
    pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::Present, GfxResourceState::RenderTarget);

    render_pass.color[0].texture = m_pSwapchain->GetBackBuffer();
    render_pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
    render_pass.depth.texture = nullptr;
    pCommandList->BeginRenderPass(render_pass);

    {
        RENDER_EVENT(pCommandList, "PostProcess");
        m_pToneMap->Draw(pCommandList, m_pHdrRT->GetSRV());
    }

    GUI* pGUI = Engine::GetInstance()->GetWorld()->GetGUI();
    pGUI->Render(pCommandList);

    pCommandList->EndRenderPass();

    pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::RenderTarget, GfxResourceState::Present);
    pCommandList->ResourceBarrier(m_pHdrRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
}

void Renderer::EndFrame()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->End();

    ++m_nCurrentFrameFenceValue;
    m_nFrameFenceValue[frame_index] = m_nCurrentFrameFenceValue;

    pCommandList->Submit();
    m_pSwapchain->Present();
    pCommandList->Signal(m_pFrameFence.get(), m_nCurrentFrameFenceValue);

    m_pStagingBufferAllocator[frame_index]->Reset();

    m_pDevice->EndFrame();
}

void Renderer::WaitGpuFinished()
{
    m_pFrameFence->Wait(m_nCurrentFrameFenceValue);
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
    m_pPointSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    m_pLinearSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pLinearSampler"));

    IGfxTexture* pBackBuffer = m_pSwapchain->GetBackBuffer();
    uint32_t width = pBackBuffer->GetDesc().width;
    uint32_t height = pBackBuffer->GetDesc().height;

    m_pDepthRT.reset(CreateRenderTarget(width, height, GfxFormat::D32FS8, "Renderer::m_pDepthRT", GfxTextureUsageDepthStencil /*| GfxTextureUsageShaderResource*/));
    m_pHdrRT.reset(CreateRenderTarget(width, height, GfxFormat::RGBA16F, "Renderer::m_pHdrRT"));

    m_pToneMap.reset(new Tonemap(this));
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height)
{
    WaitGpuFinished();

    m_pSwapchain->Resize(width, height);
}

Texture* Renderer::CreateTexture(const std::string& file)
{
    Texture* texture = new Texture(this, file);
    if (!texture->Create())
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

RenderTarget* Renderer::CreateRenderTarget(uint32_t width, uint32_t height, GfxFormat format, const std::string& name, GfxTextureUsageFlags flags, bool auto_resize, float size)
{
    RenderTarget* rt = new RenderTarget(auto_resize, size, name);
    if (!rt->Create(width, height, format, flags))
    {
        delete rt;
        return nullptr;
    }
    return rt;
}

inline void image_copy(char* dst_data, uint32_t dst_row_pitch, char* src_data, uint32_t src_row_pitch, uint32_t w, uint32_t h, uint32_t d)
{
    uint32_t src_slice_size = src_row_pitch * h;
    uint32_t dst_slice_size = dst_row_pitch * h;

    for (uint32_t z = 0; z < d; z++)
    {
        char* dst_slice = dst_data + dst_slice_size * z;
        char* src_slice = src_data + src_slice_size * z;

        for (uint32_t row = 0; row < h; ++row)
        {
            memcpy(dst_slice + dst_row_pitch * row,
                src_slice + src_row_pitch * row,
                src_row_pitch);
        }
    }
}

void Renderer::UploadTexture(IGfxTexture* texture, void* data, uint32_t data_size)
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frame_index].get();

    uint32_t required_size = texture->GetRequiredStagingBufferSize();
    StagingBuffer buffer = pAllocator->Allocate(required_size);

    const GfxTextureDesc& desc = texture->GetDesc();

    char* dst_data = (char*)buffer.buffer->GetCpuAddress() + buffer.offset;
    uint32_t dst_offset = 0;
    uint32_t src_offset = 0;

    for (uint32_t slice = 0; slice < desc.array_size; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.mip_levels; ++mip)
        {
            uint32_t min_width = GetFormatBlockWidth(desc.format);
            uint32_t min_height = GetFormatBlockHeight(desc.format);
            uint32_t w = max(desc.width >> mip, min_width);
            uint32_t h = max(desc.height >> mip, min_height);
            uint32_t d = max(desc.depth >> mip, 1);

            uint32_t src_row_pitch = GetFormatRowPitch(desc.format, w);
            uint32_t dst_row_pitch = texture->GetRowPitch(mip);

            image_copy(dst_data + dst_offset, dst_row_pitch,
                (char*)data + src_offset, src_row_pitch,
                w, h, d);

            TextureUpload upload;
            upload.texture = texture;
            upload.mip_level = mip;
            upload.array_slice = slice;
            upload.staging_buffer = buffer;
            upload.offset = dst_offset;
            m_pendingTextureUploads.push_back(upload);

            dst_offset += dst_row_pitch * h * d;
            src_offset += src_row_pitch * h * d;
        }
    }
}

void Renderer::UploadBuffer(IGfxBuffer* buffer, void* data, uint32_t data_size)
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frame_index].get();

    uint32_t required_size = buffer->GetRequiredStagingBufferSize();
    StagingBuffer staging_buffer = pAllocator->Allocate(required_size);

    char* dst_data = (char*)staging_buffer.buffer->GetCpuAddress() + staging_buffer.offset;
    memcpy(dst_data, data, data_size);

    BufferUpload upload;
    upload.buffer = buffer;
    upload.staging_buffer = staging_buffer;
    m_pendingBufferUpload.push_back(upload);
}
