#include "sky_cubemap.h"

SkyCubeMap::SkyCubeMap(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

RenderGraphHandle SkyCubeMap::Render(RenderGraph* pRenderGraph)
{
    return RenderGraphHandle();
}
