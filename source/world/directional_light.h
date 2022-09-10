#pragma once

#include "light.h"

class DirectionalLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
};