#pragma once

#include "taa.h"
#include "tonemapper.h"
#include "fxaa.h"
#include "cas.h"

struct PostProcessInput
{
    RenderGraphHandle sceneColorRT;
    RenderGraphHandle sceneDepthRT;
    RenderGraphHandle linearDepthRT;
    RenderGraphHandle velocityRT;
};

class PostProcessor
{
public:
    PostProcessor(Renderer* pRenderer);

    RenderGraphHandle Process(RenderGraph* pRenderGraph, const PostProcessInput& input, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;

    std::unique_ptr<TAA> m_pTAA;
    std::unique_ptr<Tonemapper> m_pToneMapper;
    std::unique_ptr<FXAA> m_pFXAA;
    std::unique_ptr<CAS> m_pCAS;
};