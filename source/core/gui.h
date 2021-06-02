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

    std::unique_ptr<Texture2D> m_pFontTexture;

    std::unique_ptr<StructuredBuffer> m_pVertexBuffer[MAX_INFLIGHT_FRAMES];
    std::unique_ptr<IndexBuffer> m_pIndexBuffer[MAX_INFLIGHT_FRAMES];
};