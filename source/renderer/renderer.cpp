#include "renderer.h"
#include "core/engine.h"
#include "texture_loader.h"

#include "global_constants.hlsli"

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

//test code
inline float4x4 GetLightVP(ILight* light)
{
    float3 light_dir = light->GetLightDirection();
    float3 eye = light_dir * 100.0f;
    float4x4 mtxView = lookat_matrix(eye, float3(0, 0, 0), float3(0, 1, 0), linalg::pos_z);
    float4x4 mtxProj = ortho_matrix(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 500.0f);
    float4x4 mtxVP = mul(mtxProj, mtxView);
    return mtxVP;
}

void Renderer::BeginFrame()
{
    uint32_t frame_index = m_pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;
    m_pFrameFence->Wait(m_nFrameFenceValue[frame_index]);
    m_pDevice->BeginFrame();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Begin();

    World* world = Engine::GetInstance()->GetWorld();
    Camera* camera = world->GetCamera();

    CameraConstant cameraCB;
    cameraCB.cameraPos = camera->GetPosition();
    pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 3, &cameraCB, sizeof(cameraCB));

    ILight* light = world->GetPrimaryLight();

    SceneConstant sceneCB;
    sceneCB.lightDir = light->GetLightDirection();
    sceneCB.shadowRT = m_pShadowRT->GetSRV()->GetHeapIndex();
    sceneCB.lightColor = light->GetLightColor() * light->GetLightIntensity();
    sceneCB.shadowSampler = m_pShadowSampler->GetHeapIndex();
    sceneCB.mtxLightVP = GetLightVP(light);
    sceneCB.pointRepeatSampler = m_pPointRepeatSampler->GetHeapIndex();
    sceneCB.pointClampSampler = m_pPointClampSampler->GetHeapIndex();
    sceneCB.linearRepeatSampler = m_pLinearRepeatSampler->GetHeapIndex();
    sceneCB.linearClampSampler = m_pLinearClampSampler->GetHeapIndex();
    sceneCB.envTexture = m_pEnvTexture->GetSRV()->GetHeapIndex();
    sceneCB.brdfTexture = m_pBrdfTexture->GetSRV()->GetHeapIndex();

    pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 4, &sceneCB, sizeof(sceneCB));
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

    {
        RENDER_EVENT(pCommandList, "ShadowPass");

        GfxRenderPassDesc shadow_pass;
        shadow_pass.depth.texture = m_pShadowRT->GetTexture();
        shadow_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
        shadow_pass.depth.clear_depth = 1.0f;
        pCommandList->BeginRenderPass(shadow_pass);
        pCommandList->SetViewport(0, 0, m_pShadowRT->GetTexture()->GetDesc().width, m_pShadowRT->GetTexture()->GetDesc().height);

        ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
        float4x4 mtxVP = GetLightVP(light);

        for (size_t i = 0; i < m_shadowPassBatchs.size(); ++i)
        {
            m_shadowPassBatchs[i](pCommandList, this, mtxVP);
        }
        m_shadowPassBatchs.clear();

        pCommandList->EndRenderPass();
        pCommandList->ResourceBarrier(m_pShadowRT->GetTexture(), 0, GfxResourceState::DepthStencil, GfxResourceState::ShaderResourcePSOnly);
    }

    {
        RENDER_EVENT(pCommandList, "BassPass");

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
        pCommandList->BeginRenderPass(render_pass);
        pCommandList->SetViewport(0, 0, m_pHdrRT->GetTexture()->GetDesc().width, m_pHdrRT->GetTexture()->GetDesc().height);

        for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
        {
            m_basePassBatchs[i](pCommandList, this, camera);
        }
        m_basePassBatchs.clear();

        pCommandList->EndRenderPass();
        pCommandList->ResourceBarrier(m_pHdrRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
    }

    {
        RENDER_EVENT(pCommandList, "PostProcess");
        GfxRenderPassDesc render_pass;
        render_pass.color[0].texture = m_pLdrRT->GetTexture();
        render_pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
        pCommandList->BeginRenderPass(render_pass);
        
        m_pToneMap->Draw(pCommandList, m_pHdrRT->GetSRV());

        pCommandList->EndRenderPass();
        pCommandList->ResourceBarrier(m_pLdrRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
        pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::Present, GfxResourceState::RenderTarget);

        render_pass.color[0].texture = m_pSwapchain->GetBackBuffer();
        pCommandList->BeginRenderPass(render_pass);

        m_pFXAA->Draw(pCommandList, m_pLdrRT.get());
    }

    GUI* pGUI = Engine::GetInstance()->GetWorld()->GetGUI();
    pGUI->Render(pCommandList);

    pCommandList->EndRenderPass();

    pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::RenderTarget, GfxResourceState::Present);
    pCommandList->ResourceBarrier(m_pHdrRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
    pCommandList->ResourceBarrier(m_pLdrRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
    pCommandList->ResourceBarrier(m_pShadowRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::DepthStencil);

    m_renderGraph.Clear();

    struct DepthPassData
    {
        RenderGraphHandle depthRT;
    };

    auto shadow_pass = m_renderGraph.AddPass<DepthPassData>("shadow pass",
        [](DepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            data.depthRT = builder.Create<RenderGraphTexture>("Shadow RT", desc);
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [](const DepthPassData& data, IGfxCommandList* pCommandList)
        {
        });

    struct BassPassData
    {
        RenderGraphHandle shadowRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle hdrRT;
    };

    auto base_pass = m_renderGraph.AddPass<BassPassData>("base pass",
        [&](BassPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            data.hdrRT = builder.Create<RenderGraphTexture>("HDR RT", desc);
            data.depthRT = builder.Create<RenderGraphTexture>("Depth RT", desc);

            data.shadowRT = builder.Read(shadow_pass->depthRT, GfxResourceState::ShaderResourcePSOnly);

            data.hdrRT = builder.Write(data.hdrRT, GfxResourceState::RenderTarget);
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [](const BassPassData& data, IGfxCommandList* pCommandList)
        {
        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle ldrRT;
    };

    auto tonemap_pass = m_renderGraph.AddPass<TonemapPassData>("tonemap pass",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            data.ldrRT = builder.Create<RenderGraphTexture>("LDR RT", desc);

            data.hdrRT = builder.Read(base_pass->hdrRT, GfxResourceState::ShaderResourcePSOnly);
            data.ldrRT = builder.Write(data.ldrRT, GfxResourceState::RenderTarget);
        },
        [](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
        });

    struct FXAAPassData
    {
        RenderGraphHandle ldrRT;
        RenderGraphHandle outputRT;
    };

    auto fxaa_pass = m_renderGraph.AddPass<FXAAPassData>("fxaa pass",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            data.outputRT = builder.Create<RenderGraphTexture>("Output RT", desc);

            data.ldrRT = builder.Read(tonemap_pass->ldrRT, GfxResourceState::ShaderResourcePSOnly);
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::RenderTarget);
        },
        [](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
        });

    m_renderGraph.Present(fxaa_pass->outputRT);
    m_renderGraph.Compile();
    m_renderGraph.Execute(pCommandList);
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
    m_pPointRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointRepeatSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    m_pLinearRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pLinearRepeatSampler"));

    desc.min_filter = GfxFilter::Point;
    desc.mag_filter = GfxFilter::Point;
    desc.mip_filter = GfxFilter::Point;
    desc.address_u = GfxSamplerAddressMode::ClampToEdge;
    desc.address_v = GfxSamplerAddressMode::ClampToEdge;
    desc.address_w = GfxSamplerAddressMode::ClampToEdge;
    m_pPointClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointClampSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    m_pLinearClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pLinearClampSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Point;
    desc.address_u = GfxSamplerAddressMode::ClampToEdge;
    desc.address_v = GfxSamplerAddressMode::ClampToEdge;
    desc.address_w = GfxSamplerAddressMode::ClampToEdge;
    desc.enable_compare = true;
    desc.compare_func = GfxCompareFunc::LessEqual;
    m_pShadowSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pShadowSampler"));

    void* window = Engine::GetInstance()->GetWindowHandle();
    m_pDepthRT.reset(CreateTexture2D(window, 1.0f, 1.0f, GfxFormat::D32FS8, GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource, "Renderer::m_pDepthRT"));
    m_pHdrRT.reset(CreateTexture2D(window, 1.0f, 1.0f, GfxFormat::RGBA16F, GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource, "Renderer::m_pHdrRT"));
    m_pLdrRT.reset(CreateTexture2D(window, 1.0f, 1.0f, GfxFormat::RGBA8SRGB, GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource, "Renderer::m_pLdrRT"));

    m_pShadowRT.reset(CreateTexture2D(2048, 2048, 1, GfxFormat::D16, GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource, "Renderer::m_pShadowRT"));

    m_pToneMap.reset(new Tonemap(this));
    m_pFXAA.reset(new FXAA(this));
    
    std::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pBrdfTexture.reset(CreateTexture2D(asset_path + "textures/PreintegratedGF.dds", false));
    m_pEnvTexture.reset(CreateTextureCube(asset_path + "textures/output_pmrem.dds"));
}

void Renderer::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    WaitGpuFinished();

    if (m_pSwapchain->GetDesc().window_handle == window)
    {
        m_pSwapchain->Resize(width, height);
    }
}

IndexBuffer* Renderer::CreateIndexBuffer(void* data, uint32_t stride, uint32_t index_count, const std::string& name, GfxMemoryType memory_type)
{
    IndexBuffer* buffer = new IndexBuffer(name);
    if (!buffer->Create(stride, index_count, memory_type))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), data, stride * index_count);
    }

    return buffer;
}

StructuredBuffer* Renderer::CreateStructuredBuffer(void* data, uint32_t stride, uint32_t element_count, const std::string& name, GfxMemoryType memory_type)
{
    StructuredBuffer* buffer = new StructuredBuffer(name);
    if (!buffer->Create(stride, element_count, memory_type))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), data, stride * element_count);
    }

    return buffer;
}

Texture2D* Renderer::CreateTexture2D(const std::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture2D* texture = new Texture2D(file);
    if (!texture->Create(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), GfxTextureUsageShaderResource))
    {
        delete texture;
        return nullptr;
    }

    UploadTexture(texture->GetTexture(), loader.GetData(), loader.GetDataSize());

    return texture;
}

Texture2D* Renderer::CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name)
{
    Texture2D* texture = new Texture2D(name);
    if (!texture->Create(width, height, levels, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

Texture2D* Renderer::CreateTexture2D(void* window, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name)
{
    Texture2D* texture = new Texture2D(name);
    if (!texture->Create(window, width_ratio, height_ratio, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

TextureCube* Renderer::CreateTextureCube(const std::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    TextureCube* texture = new TextureCube(file);
    if (!texture->Create(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), GfxTextureUsageShaderResource))
    {
        delete texture;
        return nullptr;
    }

    UploadTexture(texture->GetTexture(), loader.GetData(), loader.GetDataSize());

    return texture;
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
