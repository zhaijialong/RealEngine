#pragma once

#include "world.h"
#include "renderer/renderer.h"
#include "lsignal/lsignal.h"
#include "simpleini/SimpleIni.h"

class Engine
{
public:
    static Engine* GetInstance();

    void Init(const std::string& work_path, void* window_handle, uint32_t window_width, uint32_t window_height);
    void Shut();
    void Tick();

    World* GetWorld() const { return m_pWorld.get(); }
    Renderer* GetRenderer() const { return m_pRenderer.get(); }

    void* GetWindowHandle() const { return m_windowHandle; }
    const std::string& GetWorkPath() const { return m_workPath; }
    const std::string& GetAssetPath() const { return m_assetPath; }
    const std::string& GetShaderPath() const { return m_shaderPath; }

public:
    lsignal::signal<void(uint32_t, uint32_t)> WindowResizeSignal;

private:
    void LoadEngineConfig();

private:
    std::unique_ptr<Renderer> m_pRenderer;
    std::unique_ptr<World> m_pWorld;

    uint64_t m_lastFrameTime = 0;

    CSimpleIniA m_configIni;

    void* m_windowHandle = nullptr;
    std::string m_workPath;
    std::string m_assetPath;
    std::string m_shaderPath;
};