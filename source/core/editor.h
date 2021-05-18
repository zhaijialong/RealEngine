#pragma once

#include "renderer/texture.h"
#include <unordered_map>

class Editor
{
public:
    Editor();
    ~Editor();

    void Tick();

private:
    void CreateGpuMemoryStats();
    void FlushPendingTextureDeletions();

private:
    bool m_bShowGpuMemoryStats = false;
    bool m_bShowImguiDemo = false;

    std::unordered_map<IGfxDescriptor*, IGfxTexture*> m_fileDialogIcons;
    std::vector<IGfxDescriptor*> m_pendingDeletions;

    std::unique_ptr<Texture> m_pGpuMemoryStats;
};