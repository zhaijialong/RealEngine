#pragma once

#include "renderer/renderer.h"
#include <unordered_map>

class Editor
{
public:
    Editor();
    ~Editor();

    void Tick();

private:
    void DrawMenu();
    void DrawToolBar();
    void DrawGizmo();
    void DrawFrameStats();

    void CreateGpuMemoryStats();
    void FlushPendingTextureDeletions();

private:
    bool m_bShowGpuMemoryStats = false;
    bool m_bShowImguiDemo = false;

    std::unordered_map<IGfxDescriptor*, Texture2D*> m_fileDialogIcons;
    std::vector<IGfxDescriptor*> m_pendingDeletions;

    std::unique_ptr<Texture2D> m_pGpuMemoryStats;

    enum class SelectEditMode
    {
        Translate,
        Rotate,
        Scale
    };

    SelectEditMode m_selectEditMode = SelectEditMode::Translate;
    std::unique_ptr<Texture2D> m_pTranslateIcon;
    std::unique_ptr<Texture2D> m_pRotateIcon;
    std::unique_ptr<Texture2D> m_pScaleIcon;
};