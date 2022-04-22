#include "reflection_denoiser.h"

ReflectionDenoiser::ReflectionDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle ReflectionDenoiser::Render(RenderGraph* pRenderGraph)
{
    return RenderGraphHandle();
}

void ReflectionDenoiser::Reproject(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::Prefilter(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::ResolveTemporal(IGfxCommandList* pCommandList)
{
}
