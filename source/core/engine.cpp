#include "engine.h"
#include "utils/log.h"
#include "utils/profiler.h"

#define SOKOL_IMPL
#include "sokol/sokol_time.h"


Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

void Engine::Init(const std::string& work_path, void* window_handle, uint32_t window_width, uint32_t window_height)
{
    StartProfiler();

    m_windowHandle = window_handle;
    m_workPath = work_path;
    LoadEngineConfig();

    m_pRenderer = std::make_unique<Renderer>();
    m_pRenderer->CreateDevice(window_handle, window_width, window_height, m_configIni.GetBoolValue("RealEngine", "VSync"));

    m_pWorld = std::make_unique<World>();
    m_pWorld->LoadScene(m_assetPath + m_configIni.GetValue("World", "Scene"));

    m_pGUI = std::make_unique<GUI>();
    m_pGUI->Init();

    m_pEditor = std::make_unique<Editor>();

    Camera* camera = m_pWorld->GetCamera();
    camera->SetPerpective((float)window_width / window_height,
        (float)m_configIni.GetDoubleValue("Camera", "Fov"),
        (float)m_configIni.GetDoubleValue("Camera", "ZNear"),
        (float)m_configIni.GetDoubleValue("Camera", "ZFar"));

    stm_setup();
}

void Engine::Shut()
{
    ShutdownProfiler();

    //m_pWorld->SaveScene(m_assetPath + m_configIni.GetValue("World", "Scene"));
}

void Engine::Tick()
{
    CPU_EVENT("Tick", "Engine::Tick");

    float frame_time = (float)stm_sec(stm_laptime(&m_lastFrameTime));

    m_pGUI->Tick();
    m_pEditor->Tick();
    m_pWorld->Tick(frame_time);

    m_pRenderer->RenderFrame();

    TickProfiler();
}

void Engine::LoadEngineConfig()
{
    std::string ini_file = m_workPath + "RealEngine.ini";

    SI_Error error = m_configIni.LoadFile(ini_file.c_str());
    if (error != SI_OK)
    {
        RE_LOG("Failed to load RealEngine.ini !");
        return;
    }

    m_assetPath = m_workPath + m_configIni.GetValue("RealEngine", "AssetPath");
    m_shaderPath = m_workPath + m_configIni.GetValue("RealEngine", "ShaderPath");
}