#include "renderer.h"
#include "texture_loader.h"
#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include "gpu_driven_debug_line.h"
#include "gpu_driven_debug_print.h"
#include "gpu_driven_stats.h"
#include "gpu_scene.h"
#include "hierarchical_depth_buffer.h"
#include "marschner_hair_lut.h"
#include "base_pass.h"
#include "path_tracer.h"
#include "sky_cubemap.h"
#include "stbn.h"
#include "lighting/lighting_processor.h"
#include "post_processing/post_processor.h"
#include "core/engine.h"
#include "utils/profiler.h"
#include "utils/log.h"
#include "fmt/format.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "lodepng/lodepng.h"
#include "global_constants.hlsli"

Renderer::Renderer()
{
    m_pShaderCache = eastl::make_unique<ShaderCache>(this);
    m_pShaderCompiler = eastl::make_unique<ShaderCompiler>(this);
    m_pPipelineCache = eastl::make_unique<PipelineStateCache>(this);
    m_cbAllocator = eastl::make_unique<LinearAllocator>(8 * 1024 * 1024);

    Engine::GetInstance()->WindowResizeSignal.connect(&Renderer::OnWindowResize, this);
}

Renderer::~Renderer()
{
    WaitGpuFinished();
    m_pRenderGraph->Clear();

    Engine::GetInstance()->WindowResizeSignal.disconnect(this);
}

void Renderer::CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height)
{
    m_nDisplayWidth = window_width;
    m_nDisplayHeight = window_height;
    m_nRenderWidth = window_width;
    m_nRenderHeight = window_height;

    GfxDeviceDesc desc;
    desc.max_frame_lag = GFX_MAX_INFLIGHT_FRAMES;
    m_pDevice.reset(CreateGfxDevice(desc));
    if (m_pDevice == nullptr)
    {
        RE_ERROR("[Renderer::CreateDevice] failed to create the gfx device.");
        return;
    }

    GfxSwapchainDesc swapchainDesc;
    swapchainDesc.window_handle = window_handle;
    swapchainDesc.width = window_width;
    swapchainDesc.height = window_height;
    m_pSwapchain.reset(m_pDevice->CreateSwapchain(swapchainDesc, "Renderer::m_pSwapchain"));

    m_pFrameFence.reset(m_pDevice->CreateFence("Renderer::m_pFrameFence"));

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pCommandLists[{}]", i).c_str();
        m_pCommandLists[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Graphics, name));
    }

    m_pAsyncComputeFence.reset(m_pDevice->CreateFence("Renderer::m_pAsyncComputeFence"));

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pComputeCommandLists[{}]", i).c_str();
        m_pComputeCommandLists[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Compute, name));
    }

    m_pUploadFence.reset(m_pDevice->CreateFence("Renderer::m_pUploadFence"));

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pUploadCommandList[{}]", i).c_str();
        m_pUploadCommandList[i].reset(m_pDevice->CreateCommandList(GfxCommandQueue::Copy, name));

        m_pStagingBufferAllocator[i] = eastl::make_unique<StagingBufferAllocator>(this);
    }

    CreateCommonResources();

    m_pRenderGraph = eastl::make_unique<RenderGraph>(this);
    m_pGpuScene = eastl::make_unique<GpuScene>(this);
    m_pHZB = eastl::make_unique<HZB>(this);
    m_pBasePass = eastl::make_unique<BasePass>(this);
    m_pLightingProcessor = eastl::make_unique<LightingProcessor>(this);
    m_pPostProcessor = eastl::make_unique<PostProcessor>(this);
    m_pGpuDebugLine = eastl::make_unique<GpuDrivenDebugLine>(this);
    m_pGpuDebugPrint = eastl::make_unique<GpuDrivenDebugPrint>(this);
    m_pGpuStats = eastl::make_unique<GpuDrivenStats>(this);
    m_pPathTracer = eastl::make_unique<PathTracer>(this);
    m_pSkyCubeMap = eastl::make_unique<SkyCubeMap>(this);
}

void Renderer::RenderFrame()
{
    CPU_EVENT("Render", "Renderer::RenderFrame");

    BeginFrame();
    UploadResources();
    Render();
    EndFrame();

    MouseHitTest();
}

void Renderer::BeginFrame()
{
    CPU_EVENT("Render", "Renderer::BeginFrame");

    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    {
        CPU_EVENT("Render", "IGfxFence::Wait");
        m_pFrameFence->Wait(m_nFrameFenceValue[frame_index]);
    }
    m_pDevice->BeginFrame();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->ResetAllocator();
    pCommandList->Begin();
    pCommandList->BeginProfiling();

    IGfxCommandList* pComputeCommandList = m_pComputeCommandLists[frame_index].get();
    pComputeCommandList->ResetAllocator();
    pComputeCommandList->Begin();
    pComputeCommandList->BeginProfiling();

    if (m_pDevice->GetFrameID() == 0)
    {
        pCommandList->BufferBarrier(m_pSPDCounterBuffer->GetBuffer(), GfxAccessComputeUAV, GfxAccessCopyDst);
        pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);
        pCommandList->BufferBarrier(m_pSPDCounterBuffer->GetBuffer(), GfxAccessCopyDst, GfxAccessComputeUAV);
    }
}

void Renderer::UploadResources()
{
    CPU_EVENT("Render", "Renderer::UploadResources");

    if (m_pendingTextureUploads.empty() && m_pendingBufferUpload.empty())
    {
        return;
    }

    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    IGfxCommandList* pUploadCommandList = m_pUploadCommandList[frame_index].get();
    pUploadCommandList->ResetAllocator();
    pUploadCommandList->Begin();

    {
        GPU_EVENT_DEBUG(pUploadCommandList, "Renderer::UploadResources");

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
            pUploadCommandList->CopyBuffer(upload.buffer, upload.offset,
                upload.staging_buffer.buffer, upload.staging_buffer.offset, upload.staging_buffer.size);
        }
        m_pendingBufferUpload.clear();
    }

    pUploadCommandList->End();
    pUploadCommandList->Signal(m_pUploadFence.get(), ++m_nCurrentUploadFenceValue);
    pUploadCommandList->Submit();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->Wait(m_pUploadFence.get(), m_nCurrentUploadFenceValue);
}

void Renderer::FlushComputePass(IGfxCommandList* pCommandList)
{
    if (!m_animationBatchs.empty())
    {
        GPU_EVENT(pCommandList, "Animation Pass");

        m_pGpuScene->BeginAnimationUpdate(pCommandList);

        for (size_t i = 0; i < m_animationBatchs.size(); ++i)
        {
            DispatchBatch(pCommandList, m_animationBatchs[i]);
        }

        m_pGpuScene->EndAnimationUpdate(pCommandList);
    }
}

void Renderer::BuildRayTracingAS(IGfxCommandList* pGraphicsCommandList, IGfxCommandList* pComputeCommandList)
{
    if (m_bEnableAsyncCompute)
    {
        pGraphicsCommandList->End();
        pGraphicsCommandList->Signal(m_pAsyncComputeFence.get(), ++m_nCurrentAsyncComputeFenceValue);
        pGraphicsCommandList->Submit();

        pGraphicsCommandList->Begin();
        SetupGlobalConstants(pGraphicsCommandList);

        pComputeCommandList->Wait(m_pAsyncComputeFence.get(), m_nCurrentAsyncComputeFenceValue);
    }

    {
        IGfxCommandList* pCommandList = m_bEnableAsyncCompute ? pComputeCommandList : pGraphicsCommandList;
        GPU_EVENT(pCommandList, "BuildRayTracingAS");

        if (!m_pendingBLASBuilds.empty())
        {
            GPU_EVENT(pCommandList, "BuildBLAS");

            for (size_t i = 0; i < m_pendingBLASBuilds.size(); ++i)
            {
                pCommandList->BuildRayTracingBLAS(m_pendingBLASBuilds[i]);
            }
            m_pendingBLASBuilds.clear();

            pCommandList->GlobalBarrier(GfxAccessMaskAS, GfxAccessMaskAS);
        }

        if (!m_pendingBLASUpdates.empty())
        {
            GPU_EVENT(pCommandList, "UpdateBLAS");

            for (size_t i = 0; i < m_pendingBLASUpdates.size(); ++i)
            {
                pCommandList->UpdateRayTracingBLAS(m_pendingBLASUpdates[i].blas, m_pendingBLASUpdates[i].vertex_buffer, m_pendingBLASUpdates[i].vertex_buffer_offset);
            }
            m_pendingBLASUpdates.clear();

            pCommandList->GlobalBarrier(GfxAccessMaskAS, GfxAccessMaskAS);
        }

        m_pGpuScene->BuildRayTracingAS(pCommandList);
    }
}

void Renderer::SetupGlobalConstants(IGfxCommandList* pCommandList)
{
    World* world = Engine::GetInstance()->GetWorld();
    Camera* camera = world->GetCamera();
    ILight* light = world->GetPrimaryLight();

    bool enable_jitter = m_pPostProcessor->RequiresCameraJitter() || (m_outputType == RendererOutput::PathTracing);
    camera->EnableJitter(enable_jitter);

    RGHandle firstPhaseHZBHandle = m_pHZB->Get1stPhaseCullingHZBMip(0);
    RGHandle secondPhaseHZBHandle = m_pHZB->Get2ndPhaseCullingHZBMip(0);
    RGHandle sceneHZBHanlde = m_pHZB->GetSceneHZBMip(0);
    RGTexture* firstPhaseHZBTexture = m_pRenderGraph->GetTexture(firstPhaseHZBHandle);
    RGTexture* secondPhaseHZBTexture = m_pRenderGraph->GetTexture(secondPhaseHZBHandle);
    RGTexture* sceneHZBTexture = m_pRenderGraph->GetTexture(sceneHZBHanlde);

    RGHandle occlusionCulledMeshletsBufferHandle = m_pBasePass->GetSecondPhaseMeshletListBuffer();
    RGHandle occlusionCulledMeshletsCounterBufferHandle = m_pBasePass->GetSecondPhaseMeshletListCounterBuffer();
    RGBuffer* occlusionCulledMeshletsBuffer = m_pRenderGraph->GetBuffer(occlusionCulledMeshletsBufferHandle);
    RGBuffer* occlusionCulledMeshletsCounterBuffer = m_pRenderGraph->GetBuffer(occlusionCulledMeshletsCounterBufferHandle);

    SceneConstant sceneCB;
    camera->SetupCameraCB(sceneCB.cameraCB);

    sceneCB.sceneConstantBufferSRV = m_pGpuScene->GetSceneConstantSRV()->GetHeapIndex();
    sceneCB.sceneStaticBufferSRV = m_pGpuScene->GetSceneStaticBufferSRV()->GetHeapIndex();
    sceneCB.sceneAnimationBufferSRV = m_pGpuScene->GetSceneAnimationBufferSRV()->GetHeapIndex();
    sceneCB.sceneAnimationBufferUAV = m_pGpuScene->GetSceneAnimationBufferUAV()->GetHeapIndex();
    sceneCB.instanceDataAddress = m_pGpuScene->GetInstanceDataAddress();
    sceneCB.sceneRayTracingTLAS = m_pGpuScene->GetRayTracingTLASSRV()->GetHeapIndex();
    sceneCB.bShowMeshlets = m_bShowMeshlets;
    sceneCB.secondPhaseMeshletsListUAV = occlusionCulledMeshletsBuffer->GetUAV()->GetHeapIndex();
    sceneCB.secondPhaseMeshletsCounterUAV = occlusionCulledMeshletsCounterBuffer->GetUAV()->GetHeapIndex();
    sceneCB.lightDir = light->GetLightDirection();
    sceneCB.lightColor = light->GetLightColor() * light->GetLightIntensity();
    sceneCB.lightRadius = light->GetLightRadius();
    sceneCB.renderSize = uint2(m_nRenderWidth, m_nRenderHeight);
    sceneCB.rcpRenderSize = float2(1.0f / m_nRenderWidth, 1.0f / m_nRenderHeight);
    sceneCB.displaySize = uint2(m_nDisplayWidth, m_nDisplayHeight);
    sceneCB.rcpDisplaySize = float2(1.0f / m_nDisplayWidth, 1.0f / m_nDisplayHeight);
    sceneCB.prevSceneColorSRV = m_pPrevSceneColorTexture->GetSRV()->GetHeapIndex();
    sceneCB.prevSceneDepthSRV = m_pPrevSceneDepthTexture->GetSRV()->GetHeapIndex();
    sceneCB.prevNormalSRV = m_pPrevNormalTexture->GetSRV()->GetHeapIndex();
    sceneCB.HZBWidth = m_pHZB->GetHZBWidth();
    sceneCB.HZBHeight = m_pHZB->GetHZBHeight();
    sceneCB.firstPhaseCullingHZBSRV = firstPhaseHZBTexture->GetSRV()->GetHeapIndex();
    sceneCB.secondPhaseCullingHZBSRV = secondPhaseHZBTexture->GetSRV()->GetHeapIndex();
    sceneCB.sceneHZBSRV = sceneHZBTexture->IsUsed() ? sceneHZBTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
    sceneCB.debugLineDrawCommandUAV = m_pGpuDebugLine->GetArugumentsBufferUAV()->GetHeapIndex();
    sceneCB.debugLineVertexBufferUAV = m_pGpuDebugLine->GetVertexBufferUAV()->GetHeapIndex();
    sceneCB.debugTextCounterBufferUAV = m_pGpuDebugPrint->GetTextCounterBufferUAV()->GetHeapIndex();
    sceneCB.debugTextBufferUAV = m_pGpuDebugPrint->GetTextBufferUAV()->GetHeapIndex();
    sceneCB.debugFontCharBufferSRV = m_pGpuDebugPrint->GetFontCharBufferSRV()->GetHeapIndex();
    sceneCB.bEnableStats = m_bGpuDrivenStatsEnabled;
    sceneCB.statsBufferUAV = m_pGpuStats->GetStatsBufferUAV()->GetHeapIndex();
    sceneCB.minReductionSampler = m_pMinReductionSampler->GetHeapIndex();
    sceneCB.maxReductionSampler = m_pMaxReductionSampler->GetHeapIndex();
    sceneCB.pointRepeatSampler = m_pPointRepeatSampler->GetHeapIndex();
    sceneCB.pointClampSampler = m_pPointClampSampler->GetHeapIndex();
    sceneCB.bilinearRepeatSampler = m_pBilinearRepeatSampler->GetHeapIndex();
    sceneCB.bilinearClampSampler = m_pBilinearClampSampler->GetHeapIndex();
    sceneCB.bilinearBlackBoarderSampler = m_pBilinearBlackBoarderSampler->GetHeapIndex();
    sceneCB.bilinearWhiteBoarderSampler = m_pBilinearWhiteBoarderSampler->GetHeapIndex();
    sceneCB.trilinearRepeatSampler = m_pTrilinearRepeatSampler->GetHeapIndex();
    sceneCB.trilinearClampSampler = m_pTrilinearClampSampler->GetHeapIndex();
    sceneCB.aniso2xSampler = m_pAniso2xSampler->GetHeapIndex();
    sceneCB.aniso4xSampler = m_pAniso4xSampler->GetHeapIndex();
    sceneCB.aniso8xSampler = m_pAniso8xSampler->GetHeapIndex();
    sceneCB.aniso16xSampler = m_pAniso16xSampler->GetHeapIndex();
    sceneCB.skyCubeTexture = m_pSkyCubeMap->GetCubeTexture()->GetSRV()->GetHeapIndex();
    sceneCB.skySpecularIBLTexture = m_pSkyCubeMap->GetSpecularCubeTexture()->GetSRV()->GetHeapIndex();
    sceneCB.skyDiffuseIBLTexture = m_pSkyCubeMap->GetDiffuseCubeTexture()->GetSRV()->GetHeapIndex();
    sceneCB.preintegratedGFTexture = m_pPreintegratedGFTexture->GetSRV()->GetHeapIndex();
    sceneCB.blueNoiseTexture = m_pBlueNoise->GetSRV()->GetHeapIndex();
    sceneCB.sheenETexture = m_pSheenETexture->GetSRV()->GetHeapIndex();
    sceneCB.tonyMcMapfaceTexture = m_pTonyMcMapface->GetSRV()->GetHeapIndex();
    sceneCB.scalarSTBN = m_pSTBN->GetScalarTextureSRV()->GetHeapIndex();
    sceneCB.vec2STBN = m_pSTBN->GetVec2TextureSRV()->GetHeapIndex();
    sceneCB.vec3STBN = m_pSTBN->GetVec3TextureSRV()->GetHeapIndex();
    sceneCB.frameTime = Engine::GetInstance()->GetFrameDeltaTime();
    sceneCB.frameIndex = (uint32_t)GetFrameID();
    sceneCB.mipBias = m_mipBias;
    sceneCB.marschnerTextureM = m_pMarschnerHairLUT->GetM()->GetSRV()->GetHeapIndex();
    sceneCB.marschnerTextureN = m_pMarschnerHairLUT->GetN()->GetSRV()->GetHeapIndex();

    if (pCommandList->GetQueue() == GfxCommandQueue::Graphics)
    {
        pCommandList->SetGraphicsConstants(2, &sceneCB, sizeof(sceneCB));
    }

    pCommandList->SetComputeConstants(2, &sceneCB, sizeof(sceneCB));
}

void Renderer::ImportPrevFrameTextures()
{
    if (m_pPrevSceneDepthTexture == nullptr ||
        m_pPrevSceneDepthTexture->GetTexture()->GetDesc().width != m_nRenderWidth ||
        m_pPrevSceneDepthTexture->GetTexture()->GetDesc().height != m_nRenderHeight)
    {
        m_pPrevSceneDepthTexture.reset(CreateTexture2D(m_nRenderWidth, m_nRenderHeight, 1, GfxFormat::R32F, GfxTextureUsageUnorderedAccess, "Prev SceneDepth"));
        m_pPrevNormalTexture.reset(CreateTexture2D(m_nRenderWidth, m_nRenderHeight, 1, GfxFormat::RGBA8UNORM, GfxTextureUsageUnorderedAccess, "Prev Normal"));
        m_pPrevSceneColorTexture.reset(CreateTexture2D(m_nRenderWidth, m_nRenderHeight, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "Prev SceneColor"));

        m_bHistoryValid = false;
    }
    else
    {
        m_bHistoryValid = true;
    }

    m_prevSceneDepthHandle = m_pRenderGraph->Import(m_pPrevSceneDepthTexture->GetTexture(), m_bHistoryValid ? GfxAccessCopyDst : GfxAccessComputeUAV);
    m_prevNormalHandle = m_pRenderGraph->Import(m_pPrevNormalTexture->GetTexture(), m_bHistoryValid ? GfxAccessCopyDst : GfxAccessComputeUAV);
    m_prevSceneColorHandle = m_pRenderGraph->Import(m_pPrevSceneColorTexture->GetTexture(), m_bHistoryValid ? GfxAccessCopyDst : GfxAccessComputeUAV);

    if (!m_bHistoryValid)
    {
        struct ClearHistoryPassData
        {
            RGHandle linearDepth;
            RGHandle normal;
            RGHandle color;
        };

        auto clear_pass = m_pRenderGraph->AddPass<ClearHistoryPassData>("Clear Hisotry Textures", RenderPassType::Compute,
            [&](ClearHistoryPassData& data, RGBuilder& builder)
            {
                data.linearDepth = builder.Write(m_prevSceneDepthHandle);
                data.normal = builder.Write(m_prevNormalHandle);
                data.color = builder.Write(m_prevSceneColorHandle);
            },
            [=](const ClearHistoryPassData& data, IGfxCommandList* pCommandList)
            {
                float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                pCommandList->ClearUAV(m_pPrevSceneDepthTexture->GetTexture(), m_pPrevSceneDepthTexture->GetUAV(), clear_value);
                pCommandList->ClearUAV(m_pPrevNormalTexture->GetTexture(), m_pPrevNormalTexture->GetUAV(), clear_value);
                pCommandList->ClearUAV(m_pPrevSceneColorTexture->GetTexture(), m_pPrevSceneColorTexture->GetUAV(), clear_value);
            });

        m_prevSceneDepthHandle = clear_pass->linearDepth;
        m_prevNormalHandle = clear_pass->normal;
        m_prevSceneColorHandle = clear_pass->color;
    }
}

void Renderer::Render()
{
    CPU_EVENT("Render", "Renderer::Render");

    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    IGfxCommandList* pComputeCommandList = m_pComputeCommandLists[frame_index].get();

    GPU_EVENT_DEBUG(pCommandList, fmt::format("Render Frame {}", m_pDevice->GetFrameID()).c_str());

    GPU_EVENT_PROFILER(pCommandList, "Render Frame");

    m_pRenderGraph->Clear();
    m_pGpuScene->Update();
    
    ImportPrevFrameTextures();

    RGHandle outputColorHandle, outputDepthHandle;
    BuildRenderGraph(outputColorHandle, outputDepthHandle);

    m_pRenderGraph->Compile();

    m_pGpuDebugLine->Clear(pCommandList);
    m_pGpuDebugPrint->Clear(pCommandList);
    m_pGpuStats->Clear(pCommandList);

    SetupGlobalConstants(pCommandList);
    FlushComputePass(pCommandList);
    BuildRayTracingAS(pCommandList, pComputeCommandList);

    m_pSkyCubeMap->Update(pCommandList);

    World* world = Engine::GetInstance()->GetWorld();
    Camera* camera = world->GetCamera();
    camera->DrawViewFrustum(pCommandList);

    m_pRenderGraph->Execute(this, pCommandList, pComputeCommandList);

    RenderBackbufferPass(pCommandList, outputColorHandle, outputDepthHandle);
}

void Renderer::RenderBackbufferPass(IGfxCommandList* pCommandList, RGHandle color, RGHandle depth)
{
    GPU_EVENT(pCommandList, "Backbuffer Pass");

    m_pGpuStats->Draw(pCommandList);
    m_pGpuDebugPrint->PrepareForDraw(pCommandList);
    m_pGpuDebugLine->PrepareForDraw(pCommandList);

    pCommandList->TextureBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxAccessPresent, GfxAccessRTV);

    RGTexture* depthRT = m_pRenderGraph->GetTexture(depth);

    bool needUpscaleDepth = (m_upscaleMode != TemporalSuperResolution::None) && !nearly_equal(m_upscaleRatio, 1.f);
    if (needUpscaleDepth)
    {
        if (m_pUpscaledDepthTexture == nullptr ||
            m_pUpscaledDepthTexture->GetTexture()->GetDesc().width != m_nDisplayWidth ||
            m_pUpscaledDepthTexture->GetTexture()->GetDesc().height != m_nDisplayHeight)
        {
            m_pUpscaledDepthTexture.reset(CreateTexture2D(m_nDisplayWidth, m_nDisplayHeight, 1, GfxFormat::D32F, GfxTextureUsageDepthStencil, "Renderer::m_pUpscaledDepthTexture"));
        }
    }

    GfxRenderPassDesc render_pass;
    render_pass.color[0].texture = m_pSwapchain->GetBackBuffer();
    render_pass.color[0].load_op = GfxRenderPassLoadOp::DontCare;
    render_pass.depth.texture = needUpscaleDepth ? m_pUpscaledDepthTexture->GetTexture() : depthRT->GetTexture();
    render_pass.depth.load_op = needUpscaleDepth ? GfxRenderPassLoadOp::DontCare : GfxRenderPassLoadOp::Load;
    render_pass.depth.stencil_load_op = GfxRenderPassLoadOp::DontCare;
    render_pass.depth.store_op = GfxRenderPassStoreOp::DontCare;
    render_pass.depth.stencil_store_op = GfxRenderPassStoreOp::DontCare;
    render_pass.depth.read_only = true;
    pCommandList->BeginRenderPass(render_pass);

    CopyToBackbuffer(pCommandList, color, depth, needUpscaleDepth);

    m_pGpuDebugLine->Draw(pCommandList);
    m_pGpuDebugPrint->Draw(pCommandList);
    Engine::GetInstance()->GetGUI()->Render(pCommandList);

    pCommandList->EndRenderPass();
    pCommandList->TextureBarrier(m_pSwapchain->GetBackBuffer(), 0, GfxAccessRTV, GfxAccessPresent);
}

void Renderer::CopyToBackbuffer(IGfxCommandList* pCommandList, RGHandle color, RGHandle depth, bool needUpscaleDepth)
{
    GPU_EVENT(pCommandList, "CopyToBackbuffer");

    RGTexture* colorRT = m_pRenderGraph->GetTexture(color);
    RGTexture* depthRT = m_pRenderGraph->GetTexture(depth);

    uint32_t constants[3] = { colorRT->GetSRV()->GetHeapIndex(), depthRT->GetSRV()->GetHeapIndex(), m_pPointClampSampler->GetHeapIndex() };
    pCommandList->SetGraphicsConstants(0, constants, sizeof(constants));
    pCommandList->SetPipelineState(needUpscaleDepth ? m_pCopyColorDepthPSO : m_pCopyColorPSO);
    pCommandList->Draw(3);
}

void Renderer::EndFrame()
{
    CPU_EVENT("Render", "Renderer::EndFrame");

    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;

    IGfxCommandList* pComputeCommandList = m_pComputeCommandLists[frame_index].get();
    pComputeCommandList->End();
    pComputeCommandList->EndProfiling();

    IGfxCommandList* pCommandList = m_pCommandLists[frame_index].get();
    pCommandList->End();

    m_nFrameFenceValue[frame_index] = ++m_nCurrentFrameFenceValue;

    pCommandList->Signal(m_pFrameFence.get(), m_nCurrentFrameFenceValue);
    pCommandList->Submit();
    pCommandList->EndProfiling();
    {
        CPU_EVENT("Render", "IGfxSwapchain::Present");
        m_pSwapchain->Present();
    }

    m_pStagingBufferAllocator[frame_index]->Reset();
    m_cbAllocator->Reset();
    m_pGpuScene->ResetFrameData();

    m_animationBatchs.clear();
    m_forwardPassBatchs.clear();
    m_velocityPassBatchs.clear();
    m_idPassBatchs.clear();

    m_pDevice->EndFrame();
}

void Renderer::WaitGpuFinished()
{
    m_pFrameFence->Wait(m_nCurrentFrameFenceValue);
}

void Renderer::RequestMouseHitTest(uint32_t x, uint32_t y)
{
    m_nMouseX = x;
    m_nMouseY = y;
    m_bEnableObjectIDRendering = true;
}

void Renderer::MouseHitTest()
{
    if (m_bEnableObjectIDRendering)
    {
        WaitGpuFinished();

        uint32_t x = m_nMouseX;
        uint32_t y = m_nMouseY;

        if (m_upscaleMode != TemporalSuperResolution::None)
        {
            x = (uint32_t)roundf((float)m_nMouseX / m_upscaleRatio);
            y = (uint32_t)roundf((float)m_nMouseY / m_upscaleRatio);
        }

        uint8_t* data = (uint8_t*)m_pObjectIDBuffer->GetCpuAddress();
        uint32_t data_offset = m_nObjectIDRowPitch * y + x * sizeof(uint32_t);
        memcpy(&m_nMouseHitObjectID, data + data_offset, sizeof(uint32_t));

        m_bEnableObjectIDRendering = false;
    }
}

void Renderer::ReloadShaders()
{
    m_pShaderCache->ReloadShaders();
}

IGfxShader* Renderer::GetShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines, GfxShaderCompilerFlags flags)
{
    return m_pShaderCache->GetShader(file, entry_point, profile, defines, flags);
}

IGfxPipelineState* Renderer::GetPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

IGfxPipelineState* Renderer::GetPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

IGfxPipelineState* Renderer::GetPipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

void Renderer::CreateCommonResources()
{
    GfxSamplerDesc desc;
    m_pPointRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointRepeatSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    m_pBilinearRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearRepeatSampler"));

    desc.mip_filter = GfxFilter::Linear;
    m_pTrilinearRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pTrilinearRepeatSampler"));

    desc.min_filter = GfxFilter::Point;
    desc.mag_filter = GfxFilter::Point;
    desc.mip_filter = GfxFilter::Point;
    desc.address_u = GfxSamplerAddressMode::ClampToEdge;
    desc.address_v = GfxSamplerAddressMode::ClampToEdge;
    desc.address_w = GfxSamplerAddressMode::ClampToEdge;
    m_pPointClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointClampSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    m_pBilinearClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearClampSampler"));

    desc.mip_filter = GfxFilter::Linear;
    m_pTrilinearClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pTrilinearClampSampler"));

    desc.mip_filter = GfxFilter::Point;
    desc.address_u = GfxSamplerAddressMode::ClampToBorder;
    desc.address_v = GfxSamplerAddressMode::ClampToBorder;
    desc.address_w = GfxSamplerAddressMode::ClampToBorder;
    m_pBilinearBlackBoarderSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearBlackBoarderSampler"));

    desc.border_color[0] = desc.border_color[1] = desc.border_color[2] = desc.border_color[3] = 1.0f;
    m_pBilinearWhiteBoarderSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearWhiteBoarderSampler"));

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    desc.address_u = GfxSamplerAddressMode::Repeat;
    desc.address_v = GfxSamplerAddressMode::Repeat;
    desc.address_w = GfxSamplerAddressMode::Repeat;
    desc.enable_anisotropy = true;
    desc.max_anisotropy = 2.0f;
    m_pAniso2xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso2xSampler"));

    desc.max_anisotropy = 4.0f;
    m_pAniso4xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso4xSampler"));

    desc.max_anisotropy = 8.0f;
    m_pAniso8xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso8xSampler"));

    desc.max_anisotropy = 16.0f;
    m_pAniso16xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso16xSampler"));

    desc.enable_anisotropy = false;
    desc.max_anisotropy = 1.0f;

    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Point;
    desc.address_u = GfxSamplerAddressMode::ClampToEdge;
    desc.address_v = GfxSamplerAddressMode::ClampToEdge;
    desc.address_w = GfxSamplerAddressMode::ClampToEdge;
    desc.reduction_mode = GfxSamplerReductionMode::Min;
    m_pMinReductionSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pMinReductionSampler"));

    desc.reduction_mode = GfxSamplerReductionMode::Max;
    m_pMaxReductionSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pMaxReductionSampler"));

    desc.reduction_mode = GfxSamplerReductionMode::Compare;
    desc.compare_func = GfxCompareFunc::LessEqual;
    m_pShadowSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pShadowSampler"));

    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pPreintegratedGFTexture.reset(CreateTexture2D(asset_path + "textures/PreintegratedGF.dds", false));
    m_pSheenETexture.reset(CreateTexture2D(asset_path + "textures/Sheen_ash_E.dds", false));
    m_pTonyMcMapface.reset(CreateTexture3D(asset_path + "textures/tony_mc_mapface/tony_mc_mapface.dds", false));

    m_pMarschnerHairLUT = eastl::make_unique<MarschnerHairLUT>(this);
    m_pMarschnerHairLUT->Generate();

    m_pBlueNoise.reset(CreateTexture2D(asset_path + "textures/blue_noise/LDR_RGBA_0.png", false));

    m_pSTBN = eastl::make_unique<STBN>(this);
    m_pSTBN->Load(asset_path + "textures/blue_noise/STBN/");

    m_pSPDCounterBuffer.reset(CreateTypedBuffer(nullptr, GfxFormat::R32UI, 1, "Renderer::m_pSPDCounterBuffer", GfxMemoryType::GpuOnly, true));

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = GetShader("copy.hlsl", "vs_main", "vs_6_6", {});
    psoDesc.ps = GetShader("copy.hlsl", "ps_main", "ps_6_6", {});
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.rt_format[0] = m_pSwapchain->GetDesc().backbuffer_format;
    m_pCopyColorPSO = GetPipelineState(psoDesc, "Copy PSO");

    psoDesc.ps = GetShader("copy.hlsl", "ps_main", "ps_6_6", { "OUTPUT_DEPTH=1" });
    psoDesc.depthstencil_state.depth_write = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::Always;
    psoDesc.rt_format[0] = m_pSwapchain->GetDesc().backbuffer_format;
    psoDesc.depthstencil_format = GfxFormat::D32F;
    m_pCopyColorDepthPSO = GetPipelineState(psoDesc, "Copy PSO");
}

void Renderer::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    WaitGpuFinished();

    if (m_pSwapchain->GetDesc().window_handle == window)
    {
        m_pSwapchain->Resize(width, height);

        m_nDisplayWidth = width;
        m_nDisplayHeight = height;
        m_nRenderWidth = (uint32_t)((float)m_nDisplayWidth / m_upscaleRatio);
        m_nRenderHeight = (uint32_t)((float)m_nDisplayHeight / m_upscaleRatio);
    }
}

void Renderer::SetTemporalUpscaleRatio(float ratio)
{
    if (!nearly_equal(m_upscaleRatio, ratio))
    {
        m_upscaleRatio = ratio;

        m_nRenderWidth = (uint32_t)((float)m_nDisplayWidth / ratio);
        m_nRenderHeight = (uint32_t)((float)m_nDisplayHeight / ratio);

        UpdateMipBias();
    }
}

void Renderer::UpdateMipBias()
{
    if (m_nRenderWidth == m_nDisplayWidth)
    {
        m_mipBias = 0.0f;
    }
    else
    {
        m_mipBias = log2f((float)m_nRenderWidth / (float)m_nDisplayWidth) - 1.0f;
    }

    GfxSamplerDesc desc;
    desc.min_filter = GfxFilter::Linear;
    desc.mag_filter = GfxFilter::Linear;
    desc.mip_filter = GfxFilter::Linear;
    desc.address_u = GfxSamplerAddressMode::Repeat;
    desc.address_v = GfxSamplerAddressMode::Repeat;
    desc.address_w = GfxSamplerAddressMode::Repeat;
    desc.mip_bias = m_mipBias;
    desc.enable_anisotropy = true;
    desc.max_anisotropy = 2.0f;
    m_pAniso2xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso2xSampler"));

    desc.max_anisotropy = 4.0f;
    m_pAniso4xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso4xSampler"));

    desc.max_anisotropy = 8.0f;
    m_pAniso8xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso8xSampler"));

    desc.max_anisotropy = 16.0f;
    m_pAniso16xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso16xSampler"));
}

IndexBuffer* Renderer::CreateIndexBuffer(const void* data, uint32_t stride, uint32_t index_count, const eastl::string& name, GfxMemoryType memory_type)
{
    eastl::vector<uint16_t> u16IB;
    if (stride == 1)
    {
        u16IB.resize(index_count);
        for (uint32_t i = 0; i < index_count; ++i)
        {
            u16IB[i] = ((const char*)data)[i];
        }

        stride = 2;
        data = u16IB.data();
    }

    IndexBuffer* buffer = new IndexBuffer(name);
    if (!buffer->Create(stride, index_count, memory_type))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), 0, data, stride * index_count);
    }

    return buffer;
}

StructuredBuffer* Renderer::CreateStructuredBuffer(const void* data, uint32_t stride, uint32_t element_count, const eastl::string& name, GfxMemoryType memory_type, bool uav)
{
    StructuredBuffer* buffer = new StructuredBuffer(name);
    if (!buffer->Create(stride, element_count, memory_type, uav))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), 0, data, stride * element_count);
    }

    return buffer;
}

TypedBuffer* Renderer::CreateTypedBuffer(const void* data, GfxFormat format, uint32_t element_count, const eastl::string& name, GfxMemoryType memory_type, bool uav)
{
    TypedBuffer* buffer = new TypedBuffer(name);
    if (!buffer->Create(format, element_count, memory_type, uav))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), 0, data, GetFormatRowPitch(format, 1) * element_count);
    }

    return buffer;
}

RawBuffer* Renderer::CreateRawBuffer(const void* data, uint32_t size, const eastl::string& name, GfxMemoryType memory_type, bool uav)
{
    RawBuffer* buffer = new RawBuffer(name);
    if (!buffer->Create(size, memory_type, uav))
    {
        delete buffer;
        return nullptr;
    }

    if (data)
    {
        UploadBuffer(buffer->GetBuffer(), 0, data, size);
    }

    return buffer;
}

Texture2D* Renderer::CreateTexture2D(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture2D* texture = CreateTexture2D(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), 0, file);
    if (texture)
    {
        UploadTexture(texture->GetTexture(), loader.GetData());
    }

    return texture;
}

Texture2D* Renderer::CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name)
{
    Texture2D* texture = new Texture2D(name);
    if (!texture->Create(width, height, levels, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

Texture3D* Renderer::CreateTexture3D(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture3D* texture = CreateTexture3D(loader.GetWidth(), loader.GetHeight(), loader.GetDepth(), loader.GetMipLevels(), loader.GetFormat(), 0, file);
    if (texture)
    {
        UploadTexture(texture->GetTexture(), loader.GetData());
    }

    return texture;
}

Texture3D* Renderer::CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name)
{
    Texture3D* texture = new Texture3D(name);
    if (!texture->Create(width, height, depth, levels, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

TextureCube* Renderer::CreateTextureCube(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    TextureCube* texture = CreateTextureCube(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), 0, file);
    if (texture)
    {
        UploadTexture(texture->GetTexture(), loader.GetData());
    }

    return texture;
}

TextureCube* Renderer::CreateTextureCube(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name)
{
    TextureCube* texture = new TextureCube(name);
    if (!texture->Create(width, height, levels, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

Texture2DArray* Renderer::CreateTexture2DArray(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture2DArray* texture = CreateTexture2DArray(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetArraySize(), loader.GetFormat(), 0, file);
    if (texture)
    {
        UploadTexture(texture->GetTexture(), loader.GetData());
    }

    return texture;
}

Texture2DArray* Renderer::CreateTexture2DArray(uint32_t width, uint32_t height, uint32_t levels, uint32_t array_size, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name)
{
    Texture2DArray* texture = new Texture2DArray(name);
    if (!texture->Create(width, height, levels, array_size, format, flags))
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

IGfxBuffer* Renderer::GetSceneStaticBuffer() const
{
    return m_pGpuScene->GetSceneStaticBuffer();
}

OffsetAllocator::Allocation Renderer::AllocateSceneStaticBuffer(const void* data, uint32_t size)
{
    OffsetAllocator::Allocation allocation = m_pGpuScene->AllocateStaticBuffer(size);

    if (data)
    {
        UploadBuffer(m_pGpuScene->GetSceneStaticBuffer(), allocation.offset, data, size);
    }

    return allocation;
}

void Renderer::FreeSceneStaticBuffer(OffsetAllocator::Allocation allocation)
{
    m_pGpuScene->FreeStaticBuffer(allocation);
}

IGfxBuffer* Renderer::GetSceneAnimationBuffer() const
{
    return m_pGpuScene->GetSceneAnimationBuffer();
}

OffsetAllocator::Allocation Renderer::AllocateSceneAnimationBuffer(uint32_t size)
{
    return m_pGpuScene->AllocateAnimationBuffer(size);
}

void Renderer::FreeSceneAnimationBuffer(OffsetAllocator::Allocation allocation)
{
    m_pGpuScene->FreeAnimationBuffer(allocation);
}

uint32_t Renderer::AllocateSceneConstant(const void* data, uint32_t size)
{
    uint32_t address = m_pGpuScene->AllocateConstantBuffer(size);

    if (data)
    {
        void* dst = (char*)m_pGpuScene->GetSceneConstantBuffer()->GetCpuAddress() + address;
        memcpy(dst, data, size);
    }

    return address;
}

uint32_t Renderer::AddInstance(const InstanceData& data, IGfxRayTracingBLAS* blas, GfxRayTracingInstanceFlag flags)
{
    return m_pGpuScene->AddInstance(data, blas, flags);
}

inline void image_copy(char* dst_data, uint32_t dst_row_pitch, char* src_data, uint32_t src_row_pitch, uint32_t row_num, uint32_t d)
{
    uint32_t src_slice_size = src_row_pitch * row_num;
    uint32_t dst_slice_size = dst_row_pitch * row_num;

    for (uint32_t z = 0; z < d; z++)
    {
        char* dst_slice = dst_data + dst_slice_size * z;
        char* src_slice = src_data + src_slice_size * z;

        for (uint32_t row = 0; row < row_num; ++row)
        {
            memcpy(dst_slice + dst_row_pitch * row,
                src_slice + src_row_pitch * row,
                src_row_pitch);
        }
    }
}

void Renderer::UploadTexture(IGfxTexture* texture, const void* data)
{
    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
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
            uint32_t d = max(desc.depth >> mip, 1u);

            uint32_t src_row_pitch = GetFormatRowPitch(desc.format, w) * GetFormatBlockHeight(desc.format);
            uint32_t dst_row_pitch = texture->GetRowPitch(mip);

            uint32_t row_num = h / GetFormatBlockHeight(desc.format);

            image_copy(dst_data + dst_offset, dst_row_pitch,
                (char*)data + src_offset, src_row_pitch,
                row_num, d);

            TextureUpload upload;
            upload.texture = texture;
            upload.mip_level = mip;
            upload.array_slice = slice;
            upload.staging_buffer = buffer;
            upload.offset = dst_offset;
            m_pendingTextureUploads.push_back(upload);

            dst_offset += RoundUpPow2(dst_row_pitch * row_num, 512); //512 : D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
            src_offset += src_row_pitch * row_num;
        }
    }
}

void Renderer::UploadBuffer(IGfxBuffer* buffer, uint32_t offset, const void* data, uint32_t data_size)
{
    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frame_index].get();

    StagingBuffer staging_buffer = pAllocator->Allocate(data_size);

    char* dst_data = (char*)staging_buffer.buffer->GetCpuAddress() + staging_buffer.offset;
    memcpy(dst_data, data, data_size);

    BufferUpload upload;
    upload.buffer = buffer;
    upload.offset = offset;
    upload.staging_buffer = staging_buffer;
    m_pendingBufferUpload.push_back(upload);
}

void Renderer::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
    m_pendingBLASBuilds.push_back(blas);
}

void Renderer::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    m_pendingBLASUpdates.push_back({ blas, vertex_buffer, vertex_buffer_offset });
}

RenderBatch& Renderer::AddBasePassBatch()
{
    return m_pBasePass->AddBatch();
}

StagingBufferAllocator* Renderer::GetStagingBufferAllocator() const
{
    uint32_t frame_index = m_pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frame_index].get();
    return pAllocator;
}

void Renderer::SaveTexture(const eastl::string& file, const void* data, uint32_t width, uint32_t height, GfxFormat format)
{
    if (strstr(file.c_str(), ".png"))
    {
        if (format == GfxFormat::R16UNORM)
        {
            lodepng::State state;
            state.info_raw.bitdepth = 16;
            state.info_raw.colortype = LCT_GREY;
            state.info_png.color.bitdepth = 16;
            state.info_png.color.colortype = LCT_GREY;
            state.encoder.auto_convert = 0;

            eastl::vector<uint16_t> data_big_endian;
            data_big_endian.reserve(width * height);

            for (uint32_t i = 0; i < width * height; ++i)
            {
                uint16_t x = ((const uint16_t*)data)[i];
                data_big_endian.push_back(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
            }

            std::vector<unsigned char> png;
            lodepng::encode(png, (const unsigned char*)data_big_endian.data(), width, height, state);
            lodepng::save_file(png, file.c_str());
        }
        else
        {
            stbi_write_png(file.c_str(), width, height, GetFormatComponentNum(format), data, width * sizeof(uint16_t));
        }
    }
    else if (strstr(file.c_str(), ".bmp"))
    {
        stbi_write_bmp(file.c_str(), width, height, GetFormatComponentNum(format), data);
    }
    else if (strstr(file.c_str(), ".tga"))
    {
        stbi_write_tga(file.c_str(), width, height, GetFormatComponentNum(format), data);
    }
    else if (strstr(file.c_str(), ".hdr"))
    {
        stbi_write_hdr(file.c_str(), width, height, GetFormatComponentNum(format), (const float*)data);
    }
    else if (strstr(file.c_str(), ".jpg"))
    {
        stbi_write_jpg(file.c_str(), width, height, GetFormatComponentNum(format), data, 0);
    }   
}

void Renderer::SaveTexture(const eastl::string& file, IGfxTexture* texture)
{
    //todo
}
