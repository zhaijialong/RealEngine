#include "motion_blur.h"

MotionBlur::MotionBlur(Renderer* pRenderer)
{
}

RGHandle MotionBlur::Render(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth, RGHandle velocity)
{
    return sceneColor;
}
