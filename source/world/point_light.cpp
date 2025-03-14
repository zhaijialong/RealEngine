#include "point_light.h"
#include "resource_cache.h"
#include "renderer/renderer.h"
#include "core/engine.h"
#include "utils/gui_util.h"

bool PointLight::Create()
{
    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pIconTexture = ResourceCache::GetInstance()->GetTexture2D(asset_path + "ui/point_light.png");

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

    if (ImGui::CollapsingHeader("PointLight"))
    {
        ImGui::ColorEdit3("Color##Light", (float*)&m_lightColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Intensity##Light", &m_lightIntensity, 0.0f, 100.0f);
        ImGui::SliderFloat("Radius##Light", &m_lightRadius, 0.01f, 20.0f);
        ImGui::SliderFloat("Falloff##Light", &m_falloff, 1.0f, 16.0f);
    }

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(m_rotation);

    Im3d::PushMatrix(mul(T, R));
    Im3d::PushSize(3.0);
    Im3d::DrawSphere(float3(0.0f, 0.0f, 0.0f), m_lightRadius);
    Im3d::PopSize();
    Im3d::PopMatrix();
}
