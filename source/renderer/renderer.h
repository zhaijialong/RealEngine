#pragma once

#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include "staging_buffer_allocator.h"
#include "render_graph.h"
#include "resource/texture_2d.h"
#include "resource/texture_cube.h"
#include "resource/index_buffer.h"
#include "resource/structured_buffer.h"
#include "tonemap.h"
#include "fxaa.h"
#include "lsignal/lsignal.h"
#include "utils/math.h"
#include <functional>

const static int MAX_INFLIGHT_FRAMES = 3;

class Camera;
class Renderer;

using RenderFunc = std::function<void(IGfxCommandList*, Renderer*, Camera*)>;
using ShadowRenderFunc = std::function<void(IGfxCommandList*, Renderer*, const float4x4&)>;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync);
    void RenderFrame();
    void WaitGpuFinished();

    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }
    ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    uint32_t GetBackbufferWidth() const { return m_pSwapchain->GetDesc().width; }
    uint32_t GetBackbufferHeight() const { return m_pSwapchain->GetDesc().height; }

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    IGfxSwapchain* GetSwapchain() const { return m_pSwapchain.get(); }
    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);
    IGfxDescriptor* GetPointSampler() const { return m_pPointRepeatSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pLinearRepeatSampler.get(); }

    IndexBuffer* CreateIndexBuffer(void* data, uint32_t stride, uint32_t index_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly);
    StructuredBuffer* CreateStructuredBuffer(void* data, uint32_t stride, uint32_t element_count, const std::string& name, GfxMemoryType memory_type = GfxMemoryType::GpuOnly);

    Texture2D* CreateTexture2D(const std::string& file, bool srgb = true);
    Texture2D* CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name);
    Texture2D* CreateTexture2D(void* window, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags, const std::string& name);

    TextureCube* CreateTextureCube(const std::string& file, bool srgb = true);

    void UploadTexture(IGfxTexture* texture, void* data, uint32_t data_size);
    void UploadBuffer(IGfxBuffer* buffer, void* data, uint32_t data_size);

    void AddShadowPassBatch(const ShadowRenderFunc& func) { m_shadowPassBatchs.push_back(func); }
    void AddBasePassBatch(const RenderFunc& func) { m_basePassBatchs.push_back(func); }

private:
    void CreateCommonResources();
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void EndFrame();

private:
    std::unique_ptr<IGfxDevice> m_pDevice;
    std::unique_ptr<IGfxSwapchain> m_pSwapchain;

    std::unique_ptr<IGfxFence> m_pFrameFence;
    uint64_t m_nCurrentFrameFenceValue = 0;
    uint64_t m_nFrameFenceValue[MAX_INFLIGHT_FRAMES] = {};
    std::unique_ptr<IGfxCommandList> m_pCommandLists[MAX_INFLIGHT_FRAMES];

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

    std::unique_ptr<ShaderCompiler> m_pShaderCompiler;
    std::unique_ptr<ShaderCache> m_pShaderCache;
    std::unique_ptr<PipelineStateCache> m_pPipelineCache;

    std::unique_ptr<IGfxDescriptor> m_pPointRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pPointClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearRepeatSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearClampSampler;
    std::unique_ptr<IGfxDescriptor> m_pShadowSampler;

    std::unique_ptr<Texture2D> m_pBrdfTexture;
    std::unique_ptr<TextureCube> m_pEnvTexture;

    std::unique_ptr<Texture2D> m_pHdrRT;
    std::unique_ptr<Texture2D> m_pDepthRT;
    std::unique_ptr<Texture2D> m_pShadowRT;
    std::unique_ptr<Texture2D> m_pLdrRT;

    std::unique_ptr<Tonemap> m_pToneMap; //test code
    std::unique_ptr<FXAA> m_pFXAA;

    lsignal::connection m_resizeConnection;

    std::vector<ShadowRenderFunc> m_shadowPassBatchs;
    std::vector<RenderFunc> m_basePassBatchs;
};