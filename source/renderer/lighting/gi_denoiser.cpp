#include "gi_denoiser.h"

GIDenoiser::GIDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

void GIDenoiser::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
}

RGHandle GIDenoiser::Render(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    return RGHandle();
}
