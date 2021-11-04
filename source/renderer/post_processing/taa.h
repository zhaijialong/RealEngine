#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

struct TAAPassData
{
    RenderGraphHandle inputRT;
    RenderGraphHandle velocityRT;
    RenderGraphHandle outputRT;
};

struct TAACopyPassData
{
    RenderGraphHandle inputRT;
};

class TAA
{
public:
    TAA(Renderer* pRenderer);

    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height);
    void CopyHistory(IGfxCommandList* pCommandList, IGfxTexture* texture);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    std::unique_ptr<Texture2D> m_pHistoryColor;
};