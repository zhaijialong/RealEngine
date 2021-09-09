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
    m_pRenderGraph->Clear();

    Engine::GetInstance()->WindowResizeSignal.disconnect(m_resizeConnection);
}

void Renderer::CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync)
{
    m_nWindowWidth = window_width;
    m_nWindowHeight = window_height;

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

    m_pRenderGraph.reset(new RenderGraph(this));
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

    m_pRenderGraph->Clear();

    struct DepthPassData
    {
        RenderGraphHandle depthRT;
    };

    auto shadow_pass = m_pRenderGraph->AddPass<DepthPassData>("Shadow Pass",
        [](DepthPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = 2048;
            desc.height = 2048;
            desc.format = GfxFormat::D16;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;

            data.depthRT = builder.Create<RenderGraphTexture>(desc, "ShadowMap");
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [&](const DepthPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* shadowRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.depthRT);

            GfxRenderPassDesc shadow_pass;
            shadow_pass.depth.texture = shadowRT->GetTexture();
            shadow_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
            shadow_pass.depth.clear_depth = 1.0f;
            pCommandList->BeginRenderPass(shadow_pass);

            ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
            float4x4 mtxVP = GetLightVP(light);

            for (size_t i = 0; i < m_shadowPassBatchs.size(); ++i)
            {
                m_shadowPassBatchs[i](pCommandList, this, mtxVP);
            }
            m_shadowPassBatchs.clear();

            pCommandList->EndRenderPass();

            //todo : remove it
            pCommandList->ResourceBarrier(shadowRT->GetTexture(), 0, GfxResourceState::DepthStencil, GfxResourceState::ShaderResourcePSOnly);
        });

    struct BassPassData
    {
        RenderGraphHandle shadowRT;
        RenderGraphHandle depthRT;
        RenderGraphHandle hdrRT;
    };

    auto base_pass = m_pRenderGraph->AddPass<BassPassData>("Base Pass",
        [&](BassPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA16F;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.hdrRT = builder.Create<RenderGraphTexture>(desc, "SceneColor");

            desc.format = GfxFormat::D32FS8;
            desc.usage = GfxTextureUsageDepthStencil | GfxTextureUsageShaderResource;
            data.depthRT = builder.Create<RenderGraphTexture>(desc, "SceneDepth");

            data.shadowRT = builder.Read(shadow_pass->depthRT, GfxResourceState::ShaderResourcePSOnly);

            data.hdrRT = builder.Write(data.hdrRT, GfxResourceState::RenderTarget);
            data.depthRT = builder.Write(data.depthRT, GfxResourceState::DepthStencil);
        },
        [&](const BassPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* shadowMapRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.shadowRT);
            RenderGraphTexture* sceneColorRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* sceneDepthRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.depthRT);

            ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();

            SceneConstant sceneCB;
            sceneCB.lightDir = light->GetLightDirection();
            sceneCB.shadowRT = shadowMapRT->GetSRV()->GetHeapIndex();
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

            GfxRenderPassDesc render_pass;
            render_pass.color[0].texture = sceneColorRT->GetTexture();
            render_pass.color[0].load_op = GfxRenderPassLoadOp::Clear;
            render_pass.color[0].clear_color[0] = 0.0f;
            render_pass.color[0].clear_color[1] = 0.0f;
            render_pass.color[0].clear_color[2] = 0.0f;
            render_pass.color[0].clear_color[3] = 1.0f;
            render_pass.depth.texture = sceneDepthRT->GetTexture();
            render_pass.depth.load_op = GfxRenderPassLoadOp::Clear;
            render_pass.depth.stencil_load_op = GfxRenderPassLoadOp::Clear;
            pCommandList->BeginRenderPass(render_pass);

            for (size_t i = 0; i < m_basePassBatchs.size(); ++i)
            {
                m_basePassBatchs[i](pCommandList, this, camera);
            }
            m_basePassBatchs.clear();

            pCommandList->EndRenderPass();

            //todo : remove it
            pCommandList->ResourceBarrier(sceneColorRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
            pCommandList->ResourceBarrier(shadowMapRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::DepthStencil);
        });

    struct TonemapPassData
    {
        RenderGraphHandle hdrRT;
        RenderGraphHandle ldrRT;
    };

    auto tonemap_pass = m_pRenderGraph->AddPass<TonemapPassData>("Tonemapping",
        [&](TonemapPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.ldrRT = builder.Create<RenderGraphTexture>(desc, "Tonemapping Output");

            data.hdrRT = builder.Read(base_pass->hdrRT, GfxResourceState::ShaderResourcePSOnly);
            data.ldrRT = builder.Write(data.ldrRT, GfxResourceState::RenderTarget);
        },
        [&](const TonemapPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* sceneColorRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.hdrRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            GfxRenderPassDesc pass;
            pass.color[0].texture = ldrRT->GetTexture();
            pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
            pCommandList->BeginRenderPass(pass);

            m_pToneMap->Draw(pCommandList, sceneColorRT->GetSRV());

            pCommandList->EndRenderPass();

            //todo : remove it
            pCommandList->ResourceBarrier(sceneColorRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
            pCommandList->ResourceBarrier(ldrRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
        });

    struct FXAAPassData
    {
        RenderGraphHandle ldrRT;
        RenderGraphHandle outputRT;
    };

    auto fxaa_pass = m_pRenderGraph->AddPass<FXAAPassData>("FXAA",
        [&](FXAAPassData& data, RenderGraphBuilder& builder)
        {
            RenderGraphTexture::Desc desc;
            desc.width = m_nWindowWidth;
            desc.height = m_nWindowHeight;
            desc.format = GfxFormat::RGBA8SRGB;
            desc.usage = GfxTextureUsageRenderTarget | GfxTextureUsageShaderResource;
            data.outputRT = builder.Create<RenderGraphTexture>(desc, "FXAA Output");

            data.ldrRT = builder.Read(tonemap_pass->ldrRT, GfxResourceState::ShaderResourcePSOnly);
            data.outputRT = builder.Write(data.outputRT, GfxResourceState::RenderTarget);
        },
        [&](const FXAAPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* outputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.outputRT);
            RenderGraphTexture* ldrRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(data.ldrRT);

            GfxRenderPassDesc pass;
            pass.color[0].texture = outputRT->GetTexture();
            pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
            pCommandList->BeginRenderPass(pass);

            m_pFXAA->Draw(pCommandList, ldrRT->GetSRV(), m_nWindowWidth, m_nWindowHeight);

            pCommandList->EndRenderPass();

            //todo : remove it
            pCommandList->ResourceBarrier(ldrRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
            pCommandList->ResourceBarrier(outputRT->GetTexture(), 0, GfxResourceState::RenderTarget, GfxResourceState::ShaderResourcePSOnly);
        });

    m_pRenderGraph->Present(fxaa_pass->outputRT);
    m_pRenderGraph->Compile();
    m_pRenderGraph->Execute(pCommandList);


    {
        RENDER_EVENT(pCommandList, "GUI Pass");

        pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::Present, GfxResourceState::RenderTarget);

        GfxRenderPassDesc render_pass;
        render_pass.color[0].texture = m_pSwapchain->GetBackBuffer();
        render_pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
        pCommandList->BeginRenderPass(render_pass);

        RenderGraphTexture* outputRT = (RenderGraphTexture*)m_pRenderGraph->GetResource(fxaa_pass->outputRT);
        uint32_t constants[4] = { outputRT->GetSRV()->GetHeapIndex(), m_pPointClampSampler->GetHeapIndex(), 0, 0 };
        pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, constants, sizeof(constants));
        pCommandList->SetPipelineState(m_pCopyPSO);
        pCommandList->Draw(3);

        GUI* pGUI = Engine::GetInstance()->GetWorld()->GetGUI();
        pGUI->Render(pCommandList);

        pCommandList->EndRenderPass();
        pCommandList->ResourceBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxResourceState::RenderTarget, GfxResourceState::Present);

        //todo : remove it
        pCommandList->ResourceBarrier(outputRT->GetTexture(), 0, GfxResourceState::ShaderResourcePSOnly, GfxResourceState::RenderTarget);
    }
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

    m_pToneMap.reset(new Tonemap(this));
    m_pFXAA.reset(new FXAA(this));
    
    std::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pBrdfTexture.reset(CreateTexture2D(asset_path + "textures/PreintegratedGF.dds", false));
    m_pEnvTexture.reset(CreateTextureCube(asset_path + "textures/output_pmrem.dds"));

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = GetShader("copy.hlsl", "vs_main", "vs_6_6", {});
    psoDesc.ps = GetShader("copy.hlsl", "ps_main", "ps_6_6", {});
    psoDesc.rt_format[0] = m_pSwapchain->GetDesc().backbuffer_format;
    m_pCopyPSO = GetPipelineState(psoDesc, "Copy PSO");
}

void Renderer::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    WaitGpuFinished();

    if (m_pSwapchain->GetDesc().window_handle == window)
    {
        m_pSwapchain->Resize(width, height);

        m_nWindowWidth = width;
        m_nWindowHeight = height;
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
