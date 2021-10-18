#include "directional_light.h"

bool DirectionalLight::Create()
{
    return true;
}

void DirectionalLight::Tick(float delta_time)
{
    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 mtxWorld = mul(T, R);

    m_lightDir = mul(mtxWorld, float4(0.0f, 1.0f, 0.0f, 0.0f)).xyz();
}

void DirectionalLight::Render(Renderer* pRenderer)
{
}
