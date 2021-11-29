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

float4x4 DirectionalLight::GetShadowMatrix() const
{
    //temp test code
    float3 light_dir = GetLightDirection();
    float3 eye = light_dir * 100.0f;
    float4x4 mtxView = lookat_matrix(eye, float3(0, 0, 0), float3(0, 1, 0), linalg::pos_z);
    float4x4 mtxProj = ortho_matrix(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 500.0f);
    float4x4 mtxVP = mul(mtxProj, mtxView);
    return mtxVP;
}
