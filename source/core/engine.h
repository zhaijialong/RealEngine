#pragma once

#include "world/world.h"
#include "editor/editor.h"
#include "renderer/renderer.h"
#include "sigslot/signal.hpp"

namespace enki { class TaskScheduler; }

class Engine
{
public:
    static Engine* GetInstance();

    void Init(const eastl::string& work_path, void* window_handle, uint32_t window_width, uint32_t window_height);
    void Shut();
    void Tick();

    World* GetWorld() const { return m_pWorld.get(); }
    Renderer* GetRenderer() const { return m_pRenderer.get(); }
    Editor* GetEditor() const { return m_pEditor.get(); }
    enki::TaskScheduler* GetTaskScheduler() const { return m_pTaskScheduler.get(); }

    void* GetWindowHandle() const { return m_windowHandle; }
    const eastl::string& GetWorkPath() const { return m_workPath; }
    const eastl::string& GetAssetPath() const { return m_assetPath; }
    const eastl::string& GetShaderPath() const { return m_shaderPath; }

    float GetFrameDeltaTime() const { return m_frameTime; }

public:
    sigslot::signal<void*, uint32_t, uint32_t> WindowResizeSignal;

private:
    ~Engine();

private:
    eastl::unique_ptr<Renderer> m_pRenderer;
    eastl::unique_ptr<World> m_pWorld;
    eastl::unique_ptr<Editor> m_pEditor;

    eastl::unique_ptr<class enki::TaskScheduler> m_pTaskScheduler;
    
    uint64_t m_lastFrameTime = 0;
    float m_frameTime = 0.0f; //in seconds

    void* m_windowHandle = nullptr;
    eastl::string m_workPath;
    eastl::string m_assetPath;
    eastl::string m_shaderPath;
};
