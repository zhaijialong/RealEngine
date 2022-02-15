#pragma once

#include "renderer/renderer.h"
#include <unordered_map>

class Editor
{
public:
    Editor();
    ~Editor();

    void Tick();

    void AddGuiCommand(const std::string& window, const std::string& section, const std::function<void()>& command);

private:
    void DrawMenu();
    void DrawToolBar();
    void DrawGizmo();
    void DrawFrameStats();

    void CreateGpuMemoryStats();
    void CreateRenderGraph();
    void FlushPendingTextureDeletions();

    void DrawWindow(const std::string& window, bool* open);

private:
    bool m_bShowGpuMemoryStats = false;
    bool m_bShowRenderGraph = false;
    bool m_bShowImguiDemo = false;
    bool m_bShowGpuDrivenStats = false;
    bool m_bViewFrustumLocked = false;
    bool m_bVsync = true;
    bool m_bShowMeshlets = false;

    bool m_bShowInspector = false;
    bool m_bShowSettings = false;
    bool m_bShowLighting = false;
    bool m_bShowPostProcess = false;

    struct Command
    {
        std::string section;
        std::function<void()> func;
    };
    using WindowCommand = std::vector<Command>;
    std::unordered_map<std::string, WindowCommand> m_commands;

    std::unordered_map<IGfxDescriptor*, Texture2D*> m_fileDialogIcons;
    std::vector<IGfxDescriptor*> m_pendingDeletions;

    std::unique_ptr<Texture2D> m_pGpuMemoryStats;
    std::unique_ptr<Texture2D> m_pRenderGraph;

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

    float3 m_lockedViewPos;
    float3 m_lockedViewRotation;
};