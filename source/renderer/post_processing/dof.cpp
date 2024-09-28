#include "dof.h"
#include "../renderer.h"
#include "utils/gui_util.h"

DoF::DoF(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
}

void DoF::OnGui()
{
    if (ImGui::CollapsingHeader("DoF"))
    {
        ImGui::Checkbox("Enable##DoF", &m_bEnable);
    }
}

RGHandle DoF::AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height)
{
    if (!m_bEnable)
    {
        return color;
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "DoF");

    return color;
}
