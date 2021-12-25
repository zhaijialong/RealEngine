#pragma once

class Renderer;

class GpuScene
{
public:
    GpuScene(Renderer* pRenderer);

private:
    Renderer* m_pRenderer = nullptr;
};