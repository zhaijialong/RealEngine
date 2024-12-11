#pragma once

#include "light.h"

class SpotLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;
    virtual void OnGui() override;

private:
    float m_innerAngle = 0.0f;
    float m_outerAngle = 45.0f; //in degrees
};