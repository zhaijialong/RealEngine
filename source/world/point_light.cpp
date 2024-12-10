#include "point_light.h"
#include "renderer/renderer.h"
#include "core/engine.h"
#include "utils/gui_util.h"

bool PointLight::Create()
{
    return true;
}

void PointLight::Tick(float delta_time)
{
    LocalLightData data = {};
    data.type = (uint)LocalLightType::Point;
    data.position = m_pos;
    data.radius = m_lightRadius;
    data.color = m_lightColor * m_lightIntensity;
    data.falloff = m_falloff;
    data.sourceRadius = m_lightSourceRadius;

    Renderer* pRender = Engine::GetInstance()->GetRenderer();
    pRender->AddLocalLight(data);
}

bool PointLight::FrustumCull(const float4* planes, uint32_t plane_count) const
{
    return ::FrustumCull(planes, plane_count, m_pos, m_lightRadius);
}

void PointLight::OnGui()
{
    IVisibleObject::OnGui();

    GUI("Inspector", "PointLight", [&]()
        {
            ImGui::ColorEdit3("Color##Light", (float*)&m_lightColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Radius##Light", &m_lightRadius, 0.01f, 20.0f);
            ImGui::SliderFloat("Falloff##Light", &m_falloff, 1.0f, 16.0f);
        });
}
