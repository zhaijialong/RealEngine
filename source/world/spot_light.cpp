#include "spot_light.h"
#include "renderer/renderer.h"
#include "core/engine.h"

bool SpotLight::Create()
{
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

    float cosInnerAngle = cos(degree_to_radian(m_innerAngle * 0.5));
    float cosOuterAngle = cos(degree_to_radian(m_outerAngle * 0.5));
    float invAngleRange = 1.0f / max(cosInnerAngle - cosOuterAngle, 0.001f);

    data.spotAngles.x = invAngleRange;
    data.spotAngles.y = -cosOuterAngle * invAngleRange;

    Renderer* pRender = Engine::GetInstance()->GetRenderer();
    pRender->AddLocalLight(data);
}
