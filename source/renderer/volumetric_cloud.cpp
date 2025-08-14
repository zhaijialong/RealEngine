#include "volumetric_cloud.h"
#include "utils/gui_util.h"

VolumetricCloud::VolumetricCloud(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    
}

VolumetricCloud::~VolumetricCloud() = default;

void VolumetricCloud::OnGui()
{
    if (ImGui::CollapsingHeader("VolumetricCloud"))
    {
        
    }
}

RGHandle VolumetricCloud::AddPass(RenderGraph* pRenderGraph, RGHandle color, RGHandle depth, uint32_t width, uint32_t height)
{
    return color;
}
