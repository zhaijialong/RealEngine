#include "point_light.h"
#include "renderer/renderer.h"
#include "core/engine.h"

bool PointLight::Create()
{
    return true;
}

void PointLight::Tick(float delta_time)
{
    LocalLightData data;
    data.type = (uint)LocalLightType::Point;
    data.position = m_pos;
    data.direction = m_lightDir;
    data.color = m_lightColor * m_lightIntensity;
    data.radius = m_lightRadius;
    data.falloff = m_falloff;

    Renderer* pRender = Engine::GetInstance()->GetRenderer();
    pRender->AddLocalLight(data);
}
