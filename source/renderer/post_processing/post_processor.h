#pragma once

#include "tonemapper.h"
#include "fxaa.h"

struct PostProcessInput
{
    RenderGraphHandle sceneColorRT;
    RenderGraphHandle sceneDepthRT;
};

class PostProcessor
{
public:
    PostProcessor(Renderer* pRenderer);

    RenderGraphHandle Process(RenderGraph* pRenderGraph, const PostProcessInput& input, uint32_t width, uint32_t height);

private:
    std::unique_ptr<Tonemapper> m_pToneMapper;
    std::unique_ptr<FXAA> m_pFXAA;
};