#pragma once

#include "visible_object.h"

class SkySphere : public IVisibleObject
{
public:
    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;

private:
    IGfxPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer;
    eastl::unique_ptr<StructuredBuffer> m_pVertexBuffer;
};