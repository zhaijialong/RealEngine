#pragma once

#include "../render_graph.h"
#include "../resource/texture_2d.h"

struct TAAVelocityPassData
{
    RenderGraphHandle inputDepthRT;
    RenderGraphHandle inputVelocityRT;
    RenderGraphHandle outputMotionVectorRT;
};

struct TAAPassData
{
    RenderGraphHandle inputRT;
    RenderGraphHandle historyRT;
    RenderGraphHandle velocityRT;
    RenderGraphHandle linearDepthRT;
    RenderGraphHandle outputRT;
};

struct TAAApplyPassData
{
    RenderGraphHandle inputRT;
    RenderGraphHandle outputRT;
    RenderGraphHandle outputHistoryRT;
};

class TAA
{
public:
    TAA(Renderer* pRenderer);

    IGfxTexture* GetHistoryRT(uint32_t width, uint32_t height);

    void GenerateMotionVector(IGfxCommandList* pCommandList, IGfxDescriptor* depth, IGfxDescriptor* velocity, IGfxDescriptor* output, uint32_t width, uint32_t height);
    void Draw(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* velocity, IGfxDescriptor* linearDepth, IGfxDescriptor* output, uint32_t width, uint32_t height);
    void Apply(IGfxCommandList* pCommandList, IGfxDescriptor* input, IGfxDescriptor* output, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPSO = nullptr;
    IGfxPipelineState* m_pApplyPSO = nullptr;
    IGfxPipelineState* m_pMotionVectorPSO = nullptr;

    std::unique_ptr<Texture2D> m_pHistoryColor;
};