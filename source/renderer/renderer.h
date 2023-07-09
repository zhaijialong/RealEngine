#pragma once

#include "render_graph.h"
#include "render_batch.h"
#include "gpu_scene.h"
#include "resource/texture_2d.h"
#include "resource/texture_3d.h"
#include "resource/texture_cube.h"
#include "resource/index_buffer.h"
#include "resource/structured_buffer.h"
#include "resource/raw_buffer.h"
#include "resource/typed_buffer.h"
#include "staging_buffer_allocator.h"

enum class RendererOutput
{
    Default,
    Diffuse,
    Specular,
    WorldNormal,
    Roughness,
    Emissive,
    ShadingModel,
    CustomData,
    AO,
    DirectLighting,
    IndirectSpecular,
    IndirectDiffuse,
    PathTracing,
    Physics,

    Max
};

enum class TemporalSuperResolution
{
    None,
    FSR2,
    DLSS,
    XeSS,

    Max,
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height);
    void RenderFrame();
    void WaitGpuFinished();

    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }
    class ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    class ShaderCache* GetShaderCache() const { return m_pShaderCache.get(); }
    class PipelineStateCache* GetPipelineStateCache() const { return m_pPipelineCache.get(); }
    RenderGraph* GetRenderGraph() { return m_pRenderGraph.get(); }

    RendererOutput GetOutputType() const { return m_outputType; }
    void SetOutputType(RendererOutput output) { m_outputType = output; }

    TemporalSuperResolution GetTemporalUpscaleMode() const { return m_upscaleMode; }
    void SetTemporalUpscaleMode(TemporalSuperResolution mode) { m_upscaleMode = mode; }
    float GetTemporalUpscaleRatio() const { return m_upscaleRatio; }
    void SetTemporalUpscaleRatio(float ratio);

    uint32_t GetDisplayWidth() const { return m_nDisplayWidth; }
    uint32_t GetDisplayHeight() const { return m_nDisplayHeight; }
    uint32_t GetRenderWidth() const { return m_nRenderWidth; }
    uint32_t GetRenderHeight() const { return m_nRenderHeight; }

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    IGfxSwapchain* GetSwapchain() const { return m_pSwapchain.get(); }
    IGfxShader* GetShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines, GfxShaderCompilerFlags flags = 0);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name);
    IGfxPipelineState* GetPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name);
    IGfxPipelineState* GetPipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name);
    void ReloadShaders();
    IGfxDescriptor* GetPointSampler() const { return m_pPointRepeatSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pBilinearRepeatSampler.get(); }

    IndexBuffer* CreateIndexBuffer(const void* data, uint32_t stride, uint32_t index_count, const eastl::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly);
    StructuredBuffer* CreateStructuredBuffer(const void* data, uint32_t stride, uint32_t element_count, const eastl::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    TypedBuffer* CreateTypedBuffer(const void* data, GfxFormat format, uint32_t element_count, const eastl::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    RawBuffer* CreateRawBuffer(const void* data, uint32_t size, const eastl::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);

    Texture2D* CreateTexture2D(const eastl::string& file, bool srgb = true);
    Texture2D* CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name);
    Texture3D* CreateTexture3D(const eastl::string& file, bool srgb = true);
    Texture3D* CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name);
    TextureCube* CreateTextureCube(const eastl::string& file, bool srgb = true);
    TextureCube* CreateTextureCube(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const eastl::string& name);

    void SaveTexture(const eastl::string& file, const void* data, uint32_t width, uint32_t height, GfxFormat format);
    void SaveTexture(const eastl::string& file, IGfxTexture* texture);

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

    bool IsAsyncComputeEnabled() const { return m_bEnableAsyncCompute; }
    void SetAsyncComputeEnabled(bool value) { m_bEnableAsyncCompute = value; }

    void UploadTexture(IGfxTexture* texture, const void* data);
    void UploadBuffer(IGfxBuffer* buffer, uint32_t offset, const void* data, uint32_t data_size);
    void BuildRayTracingBLAS(IGfxRayTracingBLAS* blas);
    void UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset);

    LinearAllocator* GetConstantAllocator() const { return m_cbAllocator.get(); }
    RenderBatch& AddBasePassBatch();
    RenderBatch& AddForwardPassBatch() { return m_forwardPassBatchs.emplace_back(*m_cbAllocator); }
    RenderBatch& AddVelocityPassBatch() { return m_velocityPassBatchs.emplace_back(*m_cbAllocator); }
    RenderBatch& AddObjectIDPassBatch() { return m_idPassBatchs.emplace_back(*m_cbAllocator); }
    ComputeBatch& AddAnimationBatch() { return m_animationBatchs.emplace_back(*m_cbAllocator); }

    void SetupGlobalConstants(IGfxCommandList* pCommandList);

    class HZB* GetHZB() const { return m_pHZB.get(); }
    class BasePass* GetBassPass() const { return m_pBasePass.get(); }
    class SkyCubeMap* GetSkyCubeMap() const { return m_pSkyCubeMap.get(); }
    StagingBufferAllocator* GetStagingBufferAllocator() const;

    bool IsHistoryTextureValid() const { return m_bHistoryValid; }
    Texture2D* GetPrevLinearDepthTexture() const { return m_pPrevLinearDepthTexture.get(); }
    Texture2D* GetPrevNormalTexture() const { return m_pPrevNormalTexture.get(); }
    Texture2D* GetPrevSceneColorTexture() const { return m_pPrevSceneColorTexture.get(); }
    RGHandle GetPrevLinearDepthHandle() const { return m_prevLinearDepthHandle; }
    RGHandle GetPrevNormalHandle() const { return m_prevNormalHandle; }
    RGHandle GetPrevSceneColorHandle() const { return m_prevSceneColorHandle; }

    TypedBuffer* GetSPDCounterBuffer() const { return m_pSPDCounterBuffer.get(); }

private:
    void CreateCommonResources();
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth);
    void EndFrame();

    void ForwardPass(RGHandle& color, RGHandle& depth);
    RGHandle VelocityPass(RGHandle& depth);
    RGHandle LinearizeDepthPass(RGHandle depth);
    void ObjectIDPass(RGHandle& depth);
    void CopyHistoryPass(RGHandle linearDepth, RGHandle normal, RGHandle sceneColor);

    void FlushComputePass(IGfxCommandList* pCommandList);
    void BuildRayTracingAS(IGfxCommandList* pCommandList, IGfxCommandList* pComputeCommandList);
    void ImportPrevFrameTextures();
    void RenderBackbufferPass(IGfxCommandList* pCommandList, RGHandle color, RGHandle depth);
    void CopyToBackbuffer(IGfxCommandList* pCommandList, RGHandle color, RGHandle depth, bool needUpscaleDepth);
    void MouseHitTest();
    void UpdateMipBias();

private:
    eastl::unique_ptr<IGfxDevice> m_pDevice;
    eastl::unique_ptr<IGfxSwapchain> m_pSwapchain;
    eastl::unique_ptr<RenderGraph> m_pRenderGraph;
    eastl::unique_ptr<class ShaderCompiler> m_pShaderCompiler;
    eastl::unique_ptr<class ShaderCache> m_pShaderCache;
    eastl::unique_ptr<class PipelineStateCache> m_pPipelineCache;
    eastl::unique_ptr<class GpuScene> m_pGpuScene;

    RendererOutput m_outputType = RendererOutput::Default;
    TemporalSuperResolution m_upscaleMode = TemporalSuperResolution::None;

    uint32_t m_nDisplayWidth;
    uint32_t m_nDisplayHeight;
    uint32_t m_nRenderWidth;
    uint32_t m_nRenderHeight;
    float m_upscaleRatio = 1.0f;
    float m_mipBias = 0.0f;

    eastl::unique_ptr<LinearAllocator> m_cbAllocator;

    eastl::unique_ptr<IGfxFence> m_pFrameFence;
    uint64_t m_nCurrentFrameFenceValue = 0;
    uint64_t m_nFrameFenceValue[GFX_MAX_INFLIGHT_FRAMES] = {};
    eastl::unique_ptr<IGfxCommandList> m_pCommandLists[GFX_MAX_INFLIGHT_FRAMES];

    eastl::unique_ptr<IGfxFence> m_pAsyncComputeFence;
    uint64_t m_nCurrentAsyncComputeFenceValue = 0;
    eastl::unique_ptr<IGfxCommandList> m_pComputeCommandLists[GFX_MAX_INFLIGHT_FRAMES];

    eastl::unique_ptr<IGfxFence> m_pUploadFence;
    uint64_t m_nCurrentUploadFenceValue = 0;
    eastl::unique_ptr<IGfxCommandList> m_pUploadCommandList[GFX_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<StagingBufferAllocator> m_pStagingBufferAllocator[GFX_MAX_INFLIGHT_FRAMES];

    struct TextureUpload
    {
        IGfxTexture* texture;
        uint32_t mip_level;
        uint32_t array_slice;
        StagingBuffer staging_buffer;
        uint32_t offset;
    };
    eastl::vector<TextureUpload> m_pendingTextureUploads;

    struct BufferUpload
    {
        IGfxBuffer* buffer;
        uint32_t offset;
        StagingBuffer staging_buffer;
    };
    eastl::vector<BufferUpload> m_pendingBufferUpload;

    struct BLASUpdate
    {
        IGfxRayTracingBLAS* blas;
        IGfxBuffer* vertex_buffer;
        uint32_t vertex_buffer_offset;
    };
    eastl::vector<BLASUpdate> m_pendingBLASUpdates;
    eastl::vector<IGfxRayTracingBLAS*> m_pendingBLASBuilds;

    eastl::unique_ptr<IGfxDescriptor> m_pAniso2xSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pAniso4xSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pAniso8xSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pAniso16xSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pPointRepeatSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pPointClampSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pBilinearRepeatSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pBilinearClampSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pBilinearBlackBoarderSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pBilinearWhiteBoarderSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pTrilinearRepeatSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pTrilinearClampSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pShadowSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pMinReductionSampler;
    eastl::unique_ptr<IGfxDescriptor> m_pMaxReductionSampler;

    eastl::unique_ptr<Texture2D> m_pSobolSequenceTexture;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture1SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture2SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture4SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture8SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture16SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture32SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture64SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture128SPP;
    eastl::unique_ptr<Texture2D> m_pScramblingRankingTexture256SPP;

    eastl::unique_ptr<Texture2D> m_pBlueNoise;

    eastl::unique_ptr<Texture2D> m_pPreintegratedGFTexture;
    eastl::unique_ptr<Texture2D> m_pSheenETexture;
    eastl::unique_ptr<Texture3D> m_pTonyMcMapface;
    eastl::unique_ptr<class MarschnerHairLUT> m_pMarschnerHairLUT;

    eastl::unique_ptr<Texture2D> m_pPrevLinearDepthTexture;
    eastl::unique_ptr<Texture2D> m_pPrevNormalTexture;
    eastl::unique_ptr<Texture2D> m_pPrevSceneColorTexture;
    RGHandle m_prevLinearDepthHandle;
    RGHandle m_prevNormalHandle;
    RGHandle m_prevSceneColorHandle;
    bool m_bHistoryValid = false;

    eastl::unique_ptr<Texture2D> m_pUpscaledDepthTexture;
    eastl::unique_ptr<TypedBuffer> m_pSPDCounterBuffer;

    bool m_bGpuDrivenStatsEnabled = false;
    bool m_bShowMeshlets = false;
    bool m_bEnableAsyncCompute = false;

    bool m_bEnableObjectIDRendering = false;
    uint32_t m_nMouseX = 0;
    uint32_t m_nMouseY = 0;
    uint32_t m_nMouseHitObjectID = UINT32_MAX;
    eastl::unique_ptr<IGfxBuffer> m_pObjectIDBuffer;
    uint32_t m_nObjectIDRowPitch = 0;

    IGfxPipelineState* m_pCopyColorPSO = nullptr;
    IGfxPipelineState* m_pCopyColorDepthPSO = nullptr;

    eastl::unique_ptr<class HZB> m_pHZB;
    eastl::unique_ptr<class BasePass> m_pBasePass;
    eastl::unique_ptr<class LightingProcessor> m_pLightingProcessor;
    eastl::unique_ptr<class PostProcessor> m_pPostProcessor;
    eastl::unique_ptr<class PathTracer> m_pPathTracer;
    eastl::unique_ptr<class SkyCubeMap> m_pSkyCubeMap;

    eastl::unique_ptr<class GpuDrivenDebugLine> m_pGpuDebugLine;
    eastl::unique_ptr<class GpuDrivenDebugPrint> m_pGpuDebugPrint;
    eastl::unique_ptr<class GpuDrivenStats> m_pGpuStats;

    eastl::vector<ComputeBatch> m_animationBatchs;

    eastl::vector<RenderBatch> m_forwardPassBatchs;
    eastl::vector<RenderBatch> m_velocityPassBatchs;
    eastl::vector<RenderBatch> m_idPassBatchs;
};