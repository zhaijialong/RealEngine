#pragma once

#include "light.h"

class DirectionalLight : public ILight
{
public:
    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void OnGui() override;
};