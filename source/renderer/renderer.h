#pragma once

#include "render_graph.h"
#include "render_batch.h"
#include "gpu_scene.h"
#include "resource/texture_2d.h"
#include "resource/texture_cube.h"
#include "resource/index_buffer.h"
#include "resource/structured_buffer.h"
#include "resource/raw_buffer.h"
#include "resource/typed_buffer.h"
#include "staging_buffer_allocator.h"
#include "lsignal/lsignal.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height);
    void RenderFrame();
    void WaitGpuFinished();

    uint32_t GetBackbufferWidth() const { return m_pSwapchain->GetDesc().width; }
    uint32_t GetBackbufferHeight() const { return m_pSwapchain->GetDesc().height; }
    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }
    class ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    RenderGraph* GetRenderGraph() { return m_pRenderGraph.get(); }

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    IGfxSwapchain* GetSwapchain() const { return m_pSwapchain.get(); }
    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);
    IGfxPipelineState* GetPipelineState(const GfxMeshShadingPipelineDesc& desc, const std::string& name);
    IGfxPipelineState* GetPipelineState(const GfxComputePipelineDesc& desc, const std::string& name);
    IGfxDescriptor* GetPointSampler() const { return m_pPointRepeatSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pLinearRepeatSampler.get(); }

    Texture2D* GetPrevLinearDepthTexture() const { return m_pPrevLinearDepthTexture.get(); }
    RenderGraphHandle GetPrevLinearDepthHandle() const { return m_prevLinearDepthHandle; }

    IndexBuffer* CreateIndexBuffer(const void* data, uint32_t stride, uint32_t index_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly);
    StructuredBuffer* CreateStructuredBuffer(const void* data, uint32_t stride, uint32_t element_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    TypedBuffer* CreateTypedBuffer(const void* data, GfxFormat format, uint32_t element_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    RawBuffer* CreateRawBuffer(const void* data, uint32_t size, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);

    Texture2D* CreateTexture2D(const std::string& file, bool srgb = true);
    Texture2D* CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name);
    TextureCube* CreateTextureCube(const std::string& file, bool srgb = true);

    IGfxBuffer* GetSceneStaticBuffer() const;
    uint32_t AllocateSceneStaticBuffer(const void* data, uint32_t size, uint32_t alignment = 4);
    void FreeSceneStaticBuffer(uint32_t address);

    IGfxBuffer* GetSceneAnimationBuffer() const;
    uint32_t AllocateSceneAnimationBuffer(uint32_t size, uint32_t alignment = 4);
    void FreeSceneAnimationBuffer(uint32_t address);

    uint32_t AllocateSceneConstant(const void* data, uint32_t size);

    uint32_t AddInstance(const InstanceData& data, IGfxRayTracingBLAS* blas, GfxRayTracingInstanceFlag flags);
    uint32_t GetInstanceCount() const { return m_pGpuScene->GetInstanceCount(); }

    void RequestMouseHitTest(uint32_t x, uint32_t y);
    bool IsEnableMouseHitTest() const { return m_bEnableObjectIDRendering; }
    uint32_t GetMouseHitObjectID() const { return m_nMouseHitObjectID; }

    void SetGpuDrivenStatsEnabled(bool value) { m_bGpuDrivenStatsEnabled = value; }
    void SetShowMeshletsEnabled(bool value) { m_bShowMeshlets = value; }

    void UploadTexture(IGfxTexture* texture, const void* data);
    void UploadBuffer(IGfxBuffer* buffer, uint32_t offset, const void* data, uint32_t data_size);
    void BuildRayTracingBLAS(IGfxRayTracingBLAS* blas);

    LinearAllocator* GetConstantAllocator() const { return m_cbAllocator.get(); }
    RenderBatch& AddBasePassBatch();
    RenderBatch& AddForwardPassBatch() { return m_forwardPassBatchs.emplace_back(*m_cbAllocator); }
    RenderBatch& AddVelocityPassBatch() { return m_velocityPassBatchs.emplace_back(*m_cbAllocator); }
    RenderBatch& AddObjectIDPassBatch() { return m_idPassBatchs.emplace_back(*m_cbAllocator); }

    ComputeBatch& AddAnimationBatch() { return m_animationBatchs.emplace_back(*m_cbAllocator); }

    class HZB* GetHZB() const { return m_pHZB.get(); }

private:
    void CreateCommonResources();
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void BuildRenderGraph(RenderGraphHandle& outColor, RenderGraphHandle& outDepth);
    void EndFrame();

    void FlushComputePass(IGfxCommandList* pCommandList);
    void BuildRayTracingAS(IGfxCommandList* pCommandList);
    void SetupGlobalConstants(IGfxCommandList* pCommandList);
    void ImportPrevFrameTextures();
    void RenderBackbufferPass(IGfxCommandList* pCommandList, RenderGraphHandle colorRTHandle, RenderGraphHandle depthRTHandle);
    void CopyToBackbuffer(IGfxCommandList* pCommandList, RenderGraphHandle colorRTHandle);
    void MouseHitTest();

private:
    std::unique_ptr<IGfxDevice> m_pDevice;
    std::unique_ptr<IGfxSwapchain> m_pSwapchain;
    std::unique_ptr<RenderGraph> m_pRenderGraph;
    std::unique_ptr<class ShaderCompiler> m_pShaderCompiler;
    std::unique_ptr<class ShaderCache> m_pShaderCache;
    std::unique_ptr<class PipelineStateCache> m_pPipelineCache;
    std::unique_ptr<class GpuScene> m_pGpuScene;
    uint32_t m_nWindowWidth;
    uint32_t m_nWindowHeight;
    lsignal::connection m_resizeConnection;

    std::unique_ptr<LinearAllocator> m_cbAllocator;

    std::unique_ptr<IGfxFence> m_pFrameFence;
    uint64_t m_nCurrentFrameFenceValue = 0;
    uint64_t m_nFrameFenceValue[GFX_MAX_INFLIGHT_FRAMES] = {};
    std::unique_ptr<IGfxCommandList> m_pCommandLists[GFX_MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IGfxCommandList> m_pComputeCommandLists[GFX_MAX_INFLIGHT_FRAMES];

    std::unique_ptr<IGfxFence> m_pUploadFence;
    uint64_t m_nCurrentUploadFenceValue = 0;
    std::unique_ptr<IGfxCommandList> m_pUploadCommandList[GFX_MAX_INFLIGHT_FRAMES];
    std::unique_ptr<StagingBufferAllocator> m_pStagingBufferAllocator[GFX_MAX_INFLIGHT_FRAMES];

    struct TextureUpload
    {
        IGfxTexture* texture;
        uint32_t mip_level;
        uint32_t array_slice;
        StagingBuffer staging_buffer;
        uint32_t offset;
    };
    std::vector<TextureUpload> m_pendingTextureUploads;

    struct BufferUpload
    {
        IGfxBuffer* buffer;
        uint32_t offset;
        StagingBuffer staging_buffer;
    };
    std::vector<BufferUpload> m_pendingBufferUpload;

    std::vector<IGfxRayTracingBLAS*> m_pendingBLASBuilds;

    std::unique_ptr<IGfxDescriptor> m_pAniso2xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso4xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso8xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso16xSampler;
    std::unique_ptr<IGfxDescriptor> m_pPointRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pPointClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pShadowSampler;
    std::unique_ptr<IGfxDescriptor> m_pMinReductionSampler;
    std::unique_ptr<IGfxDescriptor> m_pMaxReductionSampler;

    std::unique_ptr<Texture2D> m_pBrdfTexture;
    std::unique_ptr<TextureCube> m_pEnvTexture;

    std::unique_ptr<Texture2D> m_pPrevLinearDepthTexture;
    RenderGraphHandle m_prevLinearDepthHandle;

    bool m_bGpuDrivenStatsEnabled = false;
    bool m_bShowMeshlets = false;

    bool m_bEnableObjectIDRendering = false;
    uint32_t m_nMouseX = 0;
    uint32_t m_nMouseY = 0;
    uint32_t m_nMouseHitObjectID = UINT32_MAX;
    std::unique_ptr<IGfxBuffer> m_pObjectIDBuffer;
    uint32_t m_nObjectIDRowPitch = 0;

    IGfxPipelineState* m_pCopyPSO = nullptr;

    std::unique_ptr<class HZB> m_pHZB;
    std::unique_ptr<class BasePass> m_pBasePass;
    std::unique_ptr<class LightingProcessor> m_pLightingProcessor;
    std::unique_ptr<class PostProcessor> m_pPostProcessor;

    std::unique_ptr<class GpuDrivenDebugLine> m_pGpuDebugLine;
    std::unique_ptr<class GpuDrivenDebugPrint> m_pGpuDebugPrint;
    std::unique_ptr<class GpuDrivenStats> m_pGpuStats;

    std::vector<ComputeBatch> m_animationBatchs;

    std::vector<RenderBatch> m_forwardPassBatchs;
    std::vector<RenderBatch> m_velocityPassBatchs;
    std::vector<RenderBatch> m_idPassBatchs;
};