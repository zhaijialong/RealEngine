#pragma once

#include "renderer/renderer.h"
#include "EASTL/hash_map.h"
#include "EASTL/functional.h"

class Editor
{
public:
    Editor(Renderer* pRenderer);
    ~Editor();

    void NewFrame();
    void Tick();
    void Render(IGfxCommandList* pCommandList);

private:
    void BuildDockLayout();
    void DrawMenu();
    void DrawToolBar();
    void DrawGizmo();
    void DrawFrameStats();

    void CreateGpuMemoryStats();
    void ShowRenderGraph();
    void FlushPendingTextureDeletions();

private:
    Renderer* m_pRenderer = nullptr;
    eastl::unique_ptr<class ImGuiImpl> m_pImGui;
    eastl::unique_ptr<class Im3dImpl> m_pIm3d;

    bool m_bShowGpuMemoryStats = false;
    bool m_bShowImguiDemo = false;
    bool m_bShowGpuDrivenStats = false;
    bool m_bViewFrustumLocked = false;
    bool m_bVsync = true;
    bool m_bShowMeshlets = false;
    bool m_bResetLayout = false;

    bool m_bShowInspector = true;
    bool m_bShowRenderer = true;

    unsigned int m_dockSpace = 0;

    eastl::hash_map<IGfxDescriptor*, Texture2D*> m_fileDialogIcons;
    eastl::vector<IGfxDescriptor*> m_pendingDeletions;

    eastl::unique_ptr<Texture2D> m_pGpuMemoryStats;

    enum class SelectEditMode
    {
        Translate,
        Rotate,
        Scale
    };

    SelectEditMode m_selectEditMode = SelectEditMode::Translate;
    eastl::unique_ptr<Texture2D> m_pTranslateIcon;
    eastl::unique_ptr<Texture2D> m_pRotateIcon;
    eastl::unique_ptr<Texture2D> m_pScaleIcon;

    float3 m_lockedViewPos;
    float3 m_lockedViewRotation;
};