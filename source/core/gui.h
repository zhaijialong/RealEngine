#pragma once

#include "renderer/renderer.h"
#include <memory>

class GUI
{
public:
    GUI();
    ~GUI();

    bool Init();
    void Tick();
    void Render(IGfxCommandList* pCommandList);

private:
    void SetupRenderStates(IGfxCommandList* pCommandList, uint32_t frame_index);

private:
    IGfxPipelineState* m_pPSO = nullptr;

    std::unique_ptr<IGfxTexture> m_pFontTexture;
    std::unique_ptr<IGfxDescriptor> m_pFontTextureSRV;

    std::unique_ptr<IGfxBuffer> m_pVertexBuffer[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IGfxDescriptor> m_pVertexBufferSRV[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IGfxBuffer> m_pIndexBuffer[MAX_INFLIGHT_FRAMES];
};