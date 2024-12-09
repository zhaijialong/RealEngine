#pragma once

#include "light.h"

class SpotLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);

private:
    float m_innerAngle = 0.0f;
    float m_outerAngle = 50.0f; //in degrees
};