#pragma once

#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include "staging_buffer_allocator.h"
#include "render_graph.h"
#include "gpu_debug_line.h"
#include "resource/texture_2d.h"
#include "resource/texture_cube.h"
#include "resource/index_buffer.h"
#include "resource/structured_buffer.h"
#include "resource/raw_buffer.h"
#include "lighting/lighting_processor.h"
#include "post_processing/post_processor.h"
#include "lsignal/lsignal.h"

const static int MAX_INFLIGHT_FRAMES = 3;

class ILight;
class Camera;

using ComputeFunc = std::function<void(IGfxCommandList*)>;
using ShadowFunc = std::function<void(IGfxCommandList*, const ILight*)>;
using RenderFunc = std::function<void(IGfxCommandList*, const Camera*)>;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync);
    void RenderFrame();
    void WaitGpuFinished();

    uint32_t GetBackbufferWidth() const { return m_pSwapchain->GetDesc().width; }
    uint32_t GetBackbufferHeight() const { return m_pSwapchain->GetDesc().height; }
    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }
    ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    RenderGraph* GetRenderGraph() { return m_pRenderGraph.get(); }

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    IGfxSwapchain* GetSwapchain() const { return m_pSwapchain.get(); }
    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);
    IGfxPipelineState* GetPipelineState(const GfxComputePipelineDesc& desc, const std::string& name);
    IGfxDescriptor* GetPointSampler() const { return m_pPointRepeatSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pLinearRepeatSampler.get(); }

    Texture2D* GetPrevLinearDepthTexture() const { return m_pPrevLinearDepthTexture.get(); }
    RenderGraphHandle GetPrevLinearDepthHandle() const { return m_prevLinearDepthHandle; }

    IndexBuffer* CreateIndexBuffer(void* data, uint32_t stride, uint32_t index_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly);
    StructuredBuffer* CreateStructuredBuffer(void* data, uint32_t stride, uint32_t element_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    RawBuffer* CreateRawBuffer(void* data, uint32_t size, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly, bool uav = false);
    Texture2D* CreateTexture2D(const std::string& file, bool srgb = true, bool cached = true);
    Texture2D* CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name);
    TextureCube* CreateTextureCube(const std::string& file, bool srgb = true);

    void RequestMouseHitTest(uint32_t x, uint32_t y);
    uint32_t GetMouseHitObjectID() const { return m_nMouseHitObjectID; }

    void UploadTexture(IGfxTexture* texture, void* data);
    void UploadBuffer(IGfxBuffer* buffer, void* data, uint32_t data_size);

    void AddComputePass(const ComputeFunc& func) { m_computePassBatchs.push_back(func); }
    void AddComputeBuffer(IGfxBuffer* buffer) { m_computeBuffers.push_back(buffer); }

    void AddShadowPassBatch(const ShadowFunc& func) { m_shadowPassBatchs.push_back(func); }
    void AddGBufferPassBatch(const RenderFunc& func) { m_gbufferPassBatchs.push_back(func); }
    void AddForwardPassBatch(const RenderFunc& func) { m_forwardPassBatchs.push_back(func); }
    void AddVelocityPassBatch(const RenderFunc& func) { m_velocityPassBatchs.push_back(func); }
    void AddObjectIDPassBatch(const RenderFunc& func) { m_idPassBatchs.push_back(func); }

private:
    void CreateCommonResources();
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void BuildRenderGraph(RenderGraphHandle& outColor, RenderGraphHandle& outDepth);
    void EndFrame();

    void FlushComputePass(IGfxCommandList* pCommandList);
    void SetupGlobalConstants(IGfxCommandList* pCommandList);
    void ImportPrevFrameTextures();
    void RenderBackbufferPass(IGfxCommandList* pCommandList, RenderGraphHandle colorRTHandle, RenderGraphHandle depthRTHandle);
    void CopyToBackbuffer(IGfxCommandList* pCommandList, RenderGraphHandle colorRTHandle);
    void MouseHitTest();

private:
    std::unique_ptr<IGfxDevice> m_pDevice;
    std::unique_ptr<IGfxSwapchain> m_pSwapchain;
    std::unique_ptr<RenderGraph> m_pRenderGraph;
    std::unique_ptr<ShaderCompiler> m_pShaderCompiler;
    std::unique_ptr<ShaderCache> m_pShaderCache;
    std::unique_ptr<PipelineStateCache> m_pPipelineCache;
    uint32_t m_nWindowWidth;
    uint32_t m_nWindowHeight;
    lsignal::connection m_resizeConnection;

    std::unique_ptr<IGfxFence> m_pFrameFence;
    uint64_t m_nCurrentFrameFenceValue = 0;
    uint64_t m_nFrameFenceValue[MAX_INFLIGHT_FRAMES] = {};
    std::unique_ptr<IGfxCommandList> m_pCommandLists[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IGfxCommandList> m_pComputeCommandLists[MAX_INFLIGHT_FRAMES];

    std::unique_ptr<IGfxFence> m_pUploadFence;
    uint64_t m_nCurrentUploadFenceValue = 0;
    std::unique_ptr<IGfxCommandList> m_pUploadCommandList[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<StagingBufferAllocator> m_pStagingBufferAllocator[MAX_INFLIGHT_FRAMES];

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
        StagingBuffer staging_buffer;
    };
    std::vector<BufferUpload> m_pendingBufferUpload;

    std::unique_ptr<IGfxDescriptor> m_pAniso2xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso4xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso8xSampler;
    std::unique_ptr<IGfxDescriptor> m_pAniso16xSampler;
    std::unique_ptr<IGfxDescriptor> m_pPointRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pPointClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pShadowSampler;

    Texture2D* m_pBrdfTexture;
    std::unique_ptr<TextureCube> m_pEnvTexture;

    std::unique_ptr<Texture2D> m_pPrevLinearDepthTexture;
    RenderGraphHandle m_prevLinearDepthHandle;

    bool m_bEnableObjectIDRendering = false;
    uint32_t m_nMouseX = 0;
    uint32_t m_nMouseY = 0;
    uint32_t m_nMouseHitObjectID = UINT32_MAX;
    std::unique_ptr<IGfxBuffer> m_pObjectIDBuffer;
    uint32_t m_nObjectIDRowPitch = 0;

    std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_cachedTextures;

    IGfxPipelineState* m_pCopyPSO = nullptr;

    std::unique_ptr<LightingProcessor> m_pLightingProcessor;
    std::unique_ptr<PostProcessor> m_pPostProcessor;
    std::unique_ptr<GpuDebugLine> m_pGpuDebugLine;

    std::vector<ComputeFunc> m_computePassBatchs;
    std::vector<IGfxBuffer*> m_computeBuffers;

    std::vector<ShadowFunc> m_shadowPassBatchs;
    std::vector<RenderFunc> m_gbufferPassBatchs;
    std::vector<RenderFunc> m_forwardPassBatchs;
    std::vector<RenderFunc> m_velocityPassBatchs;
    std::vector<RenderFunc> m_idPassBatchs;
};