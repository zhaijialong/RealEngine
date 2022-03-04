#pragma once

#include "taa.h"
#include "automatic_exposure.h"
#include "bloom.h"
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

    eastl::unique_ptr<TAA> m_pTAA;
    eastl::unique_ptr<AutomaticExposure> m_pAutomaticExposure;
    eastl::unique_ptr<Bloom> m_pBloom;
    eastl::unique_ptr<Tonemapper> m_pToneMapper;
    eastl::unique_ptr<FXAA> m_pFXAA;
    eastl::unique_ptr<CAS> m_pCAS;
};