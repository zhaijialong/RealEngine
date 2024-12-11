#pragma once

#include "renderer/renderer.h"
#include "EASTL/functional.h"

class ImGuiImpl
{
public:
    ImGuiImpl(Renderer* pRenderer);
    ~ImGuiImpl();

    bool Init();
    void NewFrame();
    void Render(IGfxCommandList* pCommandList);

private:
    void SetupRenderStates(IGfxCommandList* pCommandList, uint32_t frame_index);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pFontTexture;

    eastl::unique_ptr<StructuredBuffer> m_pVertexBuffer[GFX_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer[GFX_MAX_INFLIGHT_FRAMES];
};