#include "engine.h"
#include "utils/log.h"
#include "simpleini/SimpleIni.h"

Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

void Engine::Init(const std::string& work_path, void* window_handle, uint32_t window_width, uint32_t window_height)
{
    m_workPath = work_path;
    LoadEngineConfig();

    m_pWorld = std::make_unique<World>();

    m_pRenderer = std::make_unique<Renderer>();
    m_pRenderer->CreateDevice(window_handle, window_width, window_height);
}

void Engine::Shut()
{
}

void Engine::Tick()
{
    //todo : world tick, culling ...

    m_pRenderer->RenderFrame();
}

void Engine::LoadEngineConfig()
{
    std::string ini_file = m_workPath + "RealEngine.ini";

    CSimpleIniA Ini;
    SI_Error error = Ini.LoadFile(ini_file.c_str());
    if (error != SI_OK)
    {
        RE_LOG("Failed to load RealEngine.ini !");
        return;
    }

    m_assetPath = m_workPath + Ini.GetValue("RealEngine", "AssetPath");
    m_shaderPath = m_workPath + Ini.GetValue("RealEngine", "ShaderPath");
}