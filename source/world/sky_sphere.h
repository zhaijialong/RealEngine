#pragma once

#include "i_visible_object.h"

class SkySphere : public IVisibleObject
{
public:
    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
    virtual void Render(Renderer* pRenderer) override;

private:
    void RenderSky(IGfxCommandList* pCommandList, const Camera* pCamera);

private:
    IGfxPipelineState* m_pPSO = nullptr;

    std::unique_ptr<IndexBuffer> m_pIndexBuffer;
    std::unique_ptr<StructuredBuffer> m_pVertexBuffer;
};