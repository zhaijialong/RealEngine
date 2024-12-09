#include "point_light.h"
#include "renderer/renderer.h"
#include "core/engine.h"

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
