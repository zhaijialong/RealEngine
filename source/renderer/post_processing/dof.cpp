#include "dof.h"
#include "../renderer.h"
#include "utils/gui_util.h"

DoF::DoF(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
}

RGHandle DoF::AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height)
{
    GUI("PostProcess", "DoF",
        [&]()
        {
            ImGui::Checkbox("Enable##DoF", &m_bEnable);
        });

    if (!m_bEnable)
    {
        return color;
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "DoF");

    return color;
}
