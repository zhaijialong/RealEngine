#pragma once

#include "i_light.h"

class DirectionalLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
    virtual void Render(Renderer* pRenderer);

private:
    //ShadowPass*
};