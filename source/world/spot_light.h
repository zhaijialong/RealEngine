#pragma once

#include "light.h"

class SpotLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
};