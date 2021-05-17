#pragma once

#include "renderer/texture.h"

class Editor
{
public:
    void Tick();

private:
    void OpenScene();
    void CreateGpuMemoryStats();

private:
    bool m_bShowGpuMemoryStats = false;
    bool m_bShowImguiDemo = false;

    std::unique_ptr<Texture> m_pGpuMemoryStats;
};