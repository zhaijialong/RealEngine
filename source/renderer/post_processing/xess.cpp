#include "xess.h"
#include "../renderer.h"
#include "utils/gui_util.h"
#include "xess/xess_d3d12.h"

XeSS::XeSS(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    Engine::GetInstance()->WindowResizeSignal.connect(&XeSS::OnWindowResize, this);
}

XeSS::~XeSS()
{
}

RGHandle XeSS::Render(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure, 
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    return input;
}

float XeSS::GetUpscaleRatio() const
{
    return 1.0f;
}

void XeSS::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
}


