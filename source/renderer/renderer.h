#pragma once

#include "gfx/gfx.h"
#include "shader_compiler.h"
#include "shader_cache.h"
#include "pipeline_cache.h"
#include <memory>

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void *window_handle, uint32_t window_width, uint32_t window_height, bool enable_vsync);
    void RenderFrame();

    IGfxDevice* GetDevice() const { return m_pDevice.get(); }
    ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    
    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);

private:

private:
    std::unique_ptr<IGfxDevice> m_pDevice;
    std::unique_ptr<IGfxSwapchain> m_pSwapchain;

    std::unique_ptr<IGfxFence> m_pFrameFence;
    uint64_t m_nCurrentFenceValue = 0;

    const static int MAX_INFLIGHT_FRAMES = 3;
    uint64_t m_nFrameFenceValue[MAX_INFLIGHT_FRAMES] = {};
    std::unique_ptr<IGfxCommandList> m_pCommandLists[MAX_INFLIGHT_FRAMES];

    std::unique_ptr<ShaderCompiler> m_pShaderCompiler;
    std::unique_ptr<ShaderCache> m_pShaderCache;
    std::unique_ptr<PipelineStateCache> m_pPipelineCache;
};