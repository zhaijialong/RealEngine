#pragma once

#include "renderer/renderer.h"
#include <memory>

class GUI
{
public:
    GUI();
    ~GUI();

    bool Init();
    void NewFrame();
    void Render(IGfxCommandList* pCommandList);

private:
    void SetupRenderState(IGfxCommandList* pCommandList, uint32_t frame_index);

private:
    IGfxPipelineState* m_pPSO = nullptr;

    std::unique_ptr<IGfxTexture> m_pFontTexture;
    uint32_t m_fontTextureSRV = GFX_INVALID_RESOURCE;

    std::unique_ptr<IGfxBuffer> m_pVertexBuffer[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IGfxBuffer> m_pIndexBuffer[MAX_INFLIGHT_FRAMES];
    uint32_t m_vertexBufferSRV[MAX_INFLIGHT_FRAMES] = { GFX_INVALID_RESOURCE, GFX_INVALID_RESOURCE, GFX_INVALID_RESOURCE };
};