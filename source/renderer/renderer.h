#pragma once

#include "gfx/gfx.h"
#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include "staging_buffer_allocator.h"
#include "lsignal/lsignal.h"

const static int MAX_INFLIGHT_FRAMES = 3;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync);
    void RenderFrame();
    void WaitGpuFinished();

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    IGfxSwapchain* GetSwapchain() const { return m_pSwapchain.get(); }
    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);
    IGfxDescriptor* GetPointSampler() const { return m_pPointSampler.get(); }
    IGfxDescriptor* GetLinearSampler() const { return m_pLinearSampler.get(); }

    ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }

    void UploadTexture(IGfxTexture* texture, void* data, uint32_t data_size);
    void UploadBuffer(IGfxBuffer* buffer, void* data, uint32_t data_size);

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

    };
    std::vector<BufferUpload> m_pendingBufferUpload;

    std::unique_ptr<ShaderCompiler> m_pShaderCompiler;
    std::unique_ptr<ShaderCache> m_pShaderCache;
    std::unique_ptr<PipelineStateCache> m_pPipelineCache;

    std::unique_ptr<IGfxDescriptor> m_pPointSampler;
    std::unique_ptr<IGfxDescriptor> m_pLinearSampler;

    lsignal::connection m_resizeConnection;
};