#pragma once

#include "light.h"

class PointLight : public ILight
{
public:
    virtual bool Create();
    virtual void Tick(float delta_time);
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const override;
    virtual void OnGui() override;
};