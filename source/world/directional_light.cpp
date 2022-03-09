#include "directional_light.h"
#include "utils/gui_util.h"

bool DirectionalLight::Create()
{
    return true;
}

void DirectionalLight::Tick(float delta_time)
{
    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 mtxWorld = mul(T, R);

    m_lightDir = normalize(mul(mtxWorld, float4(0.0f, 1.0f, 0.0f, 0.0f)).xyz());

    GUI("Settings", "Sun Light", [&]()
        {
            ImGui::SliderFloat3("Rotation##DirectionalLight", &m_rotation.x, -180.0f, 180.0);
            ImGui::SliderFloat("Radius##DirectionalLight", &m_lightRadius, 0.0f, 0.1f);
            ImGui::SliderFloat("Intensity##DirectionalLight", &m_lightIntensity, 0.0f, 100.0f);
            ImGui::ColorEdit3("Color##DirectionalLight", &m_lightColor.x);
        });
}
