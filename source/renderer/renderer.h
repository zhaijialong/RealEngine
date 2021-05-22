#pragma once

#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include "staging_buffer_allocator.h"
#include "texture.h"
#include "render_target.h"
#include "tonemap.h"
#include "lsignal/lsignal.h"
#include <functional>

const static int MAX_INFLIGHT_FRAMES = 3;

class Camera;
class Renderer;

using RenderFunc = std::function<void(IGfxCommandList*, Renderer*, Camera*)>;

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
    IGfxDescriptor* GetPointSampler() const { return m_pPointSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pLinearSampler.get(); }

    Texture* CreateTexture(const std::string& file);
    RenderTarget* CreateRenderTarget(uint32_t width, uint32_t height, GfxFormat format, const std::string& name,
        GfxTextureUsageFlags flags = GfxTextureUsageShaderResource | GfxTextureUsageRenderTarget, bool auto_resize = true, float size = 1.0f);

    void UploadTexture(IGfxTexture* texture, void* data, uint32_t data_size);
    void UploadBuffer(IGfxBuffer* buffer, void* data, uint32_t data_size);

    void AddBasePassBatch(const RenderFunc& func) { m_basePassBatchs.push_back(func); }

private:
    void CreateCommonResources();
    void OnWindowResize(uint32_t width, uint32_t height);

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

    std::unique_ptr<IGfxDescriptor> m_pPointSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearSampler;

    std::unique_ptr<RenderTarget> m_pHdrRT;
    std::unique_ptr<RenderTarget> m_pDepthRT;

    std::unique_ptr<Tonemap> m_pToneMap; //test code

    lsignal::connection m_resizeConnection;

    std::vector<RenderFunc> m_basePassBatchs;
};