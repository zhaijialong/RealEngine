#pragma once

#include "i_light.h"

class DirectionalLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
    virtual void Render(Renderer* pRenderer);
    
    virtual float4x4 GetShadowMatrix() const override;

private:
    
};