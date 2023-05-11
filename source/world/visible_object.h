#pragma once

#include "renderer/renderer.h"
#include "utils/math.h"

class IVisibleObject
{
public:
    virtual ~IVisibleObject() {}

    virtual bool Create() = 0;
    virtual void Tick(float delta_time) = 0;
    virtual void Render(Renderer* pRenderer) {}
    virtual bool FrustumCull(const float4* planes, uint32_t plane_count) const { return true; }
    virtual void OnGui();

    virtual float3 GetPosition() const { return m_pos; }
    virtual void SetPosition(const float3& pos) { m_pos = pos; }

    virtual quaternion GetRotation() const { return m_rotation; }
    virtual void SetRotation(const quaternion& rotation) { m_rotation = rotation; }

    virtual float3 GetScale() const { return m_scale; }
    virtual void SetScale(const float3& scale) { m_scale = scale; }

    void SetID(uint32_t id) { m_nID = id; }

protected:
    uint32_t m_nID = 0;
    float3 m_pos = { 0.0f, 0.0f, 0.0f };
    quaternion m_rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    float3 m_scale = { 1.0f, 1.0f, 1.0f };
};