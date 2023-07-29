#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class TAA
{
public:
    TAA(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColorRT, RGHandle sceneDepthRT, RGHandle velocityRT, uint32_t width, uint32_t height);

    bool IsEnabled() const { return m_bEnable; }

private:
    void Draw(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* velocity, RGTexture* depth, RGTexture* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;

    //ping-pong
    eastl::unique_ptr<Texture2D> m_pHistoryColorInput;
    eastl::unique_ptr<Texture2D> m_pHistoryColorOutput;

    bool m_bHistoryInvalid = true;
    bool m_bEnable = true;
};