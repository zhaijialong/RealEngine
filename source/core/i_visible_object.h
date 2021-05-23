#pragma once

#include "renderer/renderer.h"
#include "utils/math.h"
#include "tinyxml2/tinyxml2.h"

class IVisibleObject
{
public:
    virtual ~IVisibleObject() {}

    virtual void Load(tinyxml2::XMLElement* element);
    virtual void Store(tinyxml2::XMLElement* element);

    virtual bool Create() = 0;
    virtual void Tick(float delta_time) = 0;
    virtual void Render(Renderer* pRenderer) = 0;

    float3 GetPosition() const { return m_pos; }
    void SetPosition(const float3& pos) { m_pos = pos; }

    float3 GetRotation() const { return m_rotation; }
    void SetRotation(const float3& rotation) { m_rotation = rotation; }

    float3 GetScale() const { return m_scale; }
    void SetScale(const float3& scale) { m_scale = scale; }

protected:
    float3 m_pos = { 0.0f, 0.0f, 0.0f };
    float3 m_rotation = { 0.0f, 0.0f, 0.0f }; //in degrees
    float3 m_scale = { 1.0f, 1.0f, 1.0f };
};