#pragma once

class Renderer;

class PipelineStateCache
{
public:
    PipelineStateCache(Renderer* pRenderer);

private:
    Renderer* m_pRenderer;
};