#pragma once

#include "light.h"

class PointLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
};