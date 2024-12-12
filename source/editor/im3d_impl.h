#pragma once

#include "renderer/renderer.h"

class Im3dImpl
{
public:
    Im3dImpl(Renderer* pRenderer);
    ~Im3dImpl();

    bool Init();
    void NewFrame();
    void Render(IGfxCommandList* pCommandList);

private:
    uint32_t GetVertexCount() const;

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPointPSO = nullptr;
    IGfxPipelineState* m_pLinePSO = nullptr;
    IGfxPipelineState* m_pTrianglePSO = nullptr;

    eastl::unique_ptr<RawBuffer> m_pVertexBuffer[GFX_MAX_INFLIGHT_FRAMES];
};