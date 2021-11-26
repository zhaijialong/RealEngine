#pragma once

#include "i_visible_object.h"

class ILight : public IVisibleObject
{
public:
    float3 GetLightDirection() const { return m_lightDir; }

    float3 GetLightColor() const { return m_lightColor; }
    void SetLightColor(const float3& color) { m_lightColor = color; }

    float GetLightIntensity() const { return m_lightIntensity; }
    void SetLightIntensity(float intensity) { m_lightIntensity = intensity; }

protected:
    float3 m_lightDir = { 0.0f, 1.0f, 0.0f };
    float3 m_lightColor = { 1.0f, 1.0f, 1.0f };
    float m_lightIntensity = 1.0f;
};