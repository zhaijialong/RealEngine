#include "spot_light.h"
#include "resource_cache.h"
#include "renderer/renderer.h"
#include "core/engine.h"
#include "utils/gui_util.h"

bool SpotLight::Create()
{
    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pIconTexture = ResourceCache::GetInstance()->GetTexture2D(asset_path + "ui/spot_light.png");

    return true;
}

void SpotLight::Tick(float delta_time)
{
    float4x4 R = rotation_matrix(m_rotation);
    m_lightDir = normalize(mul(R, float4(0.0f, 1.0f, 0.0f, 0.0f)).xyz());

    LocalLightData data = {};
    data.type = (uint)LocalLightType::Spot;
    data.position = m_pos;
    data.radius = m_lightRadius;
    data.color = m_lightColor * m_lightIntensity;
    data.falloff = m_falloff;
    data.direction = m_lightDir;
    data.sourceRadius = m_lightSourceRadius;

    float cosInnerAngle = cos(degree_to_radian(m_innerAngle));
    float cosOuterAngle = cos(degree_to_radian(m_outerAngle));
    float invAngleRange = 1.0f / max(cosInnerAngle - cosOuterAngle, 0.001f);

    data.spotAngles.x = invAngleRange;
    data.spotAngles.y = -cosOuterAngle * invAngleRange;
    data.spotAngles.z = degree_to_radian(m_outerAngle);

    Renderer* pRender = Engine::GetInstance()->GetRenderer();
    pRender->AddLocalLight(data);
}

bool SpotLight::FrustumCull(const float4* planes, uint32_t plane_count) const
{
    float4 boundingSphere = ConeBoundingSphere(m_pos, -m_lightDir, m_lightRadius, degree_to_radian(m_outerAngle));
    return ::FrustumCull(planes, plane_count, boundingSphere.xyz(), boundingSphere.w);
}

void SpotLight::OnGui()
{
    IVisibleObject::OnGui();

    GUI("Inspector", "SpotLight", [&]()
        {
            ImGui::ColorEdit3("Color##Light", (float*)&m_lightColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Intensity##Light", &m_lightIntensity, 0.0f, 100.0f);
            ImGui::SliderFloat("Radius##Light", &m_lightRadius, 0.01f, 20.0f);
            ImGui::SliderFloat("Falloff##Light", &m_falloff, 1.0f, 16.0f);
            ImGui::SliderFloat("Inner Angle##Light", &m_innerAngle, 0.0f, 90.0f);
            ImGui::SliderFloat("Outer Angle##Light", &m_outerAngle, 0.0f, 90.0f);
        });

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(m_rotation);

    float height = cos(degree_to_radian(m_outerAngle)) * m_lightRadius;
    float radius = sin(degree_to_radian(m_outerAngle)) * m_lightRadius;

    Im3d::PushMatrix(mul(T, R));
    Im3d::PushSize(3.0);
    Im3d::DrawCone(float3(0.0f, -height, 0.0f), float3(0.0f, 1.0f, 0.0f), height, radius, 30);
    Im3d::PopSize();
    Im3d::PopMatrix();
}
