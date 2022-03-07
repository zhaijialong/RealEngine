#include "engine.h"
#include "utils/log.h"
#include "utils/profiler.h"
#include "enkiTS/TaskScheduler.h"
#include "rpmalloc/rpmalloc.h"
#include "rpmalloc/rpnew.h"

#define SOKOL_IMPL
#include "sokol/sokol_time.h"

Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

Engine::~Engine()
{
    rpmalloc_finalize();
}

void Engine::Init(const eastl::string& work_path, void* window_handle, uint32_t window_width, uint32_t window_height)
{
    StartProfiler();

    enki::TaskSchedulerConfig config;
    config.profilerCallbacks.threadStart = [](uint32_t) { rpmalloc_thread_initialize(); };
    config.profilerCallbacks.threadStop = [](uint32_t) { rpmalloc_thread_finalize(1); };

    m_pTaskScheduler.reset(new enki::TaskScheduler());
    m_pTaskScheduler->Initialize(config);

    m_windowHandle = window_handle;
    m_workPath = work_path;
    LoadEngineConfig();

    m_pRenderer = eastl::make_unique<Renderer>();
    m_pRenderer->CreateDevice(window_handle, window_width, window_height);

    m_pWorld = eastl::make_unique<World>();
    m_pWorld->LoadScene(m_assetPath + m_configIni.GetValue("World", "Scene"));

    m_pGUI = eastl::make_unique<GUI>();
    m_pGUI->Init();

    m_pEditor = eastl::make_unique<Editor>();

    stm_setup();
}

void Engine::Shut()
{
    ShutdownProfiler();

    m_pTaskScheduler->WaitforAllAndShutdown();
    m_pTaskScheduler.reset();

    m_pWorld.reset();
    m_pEditor.reset();
    m_pGUI.reset();
    m_pRenderer.reset();
}

void Engine::Tick()
{
    CPU_EVENT("Tick", "Engine::Tick");

    m_frameTime = (float)stm_sec(stm_laptime(&m_lastFrameTime));

    m_pGUI->Tick();
    m_pEditor->Tick();
    m_pWorld->Tick(m_frameTime);

    m_pRenderer->RenderFrame();

    TickProfiler();
}

void Engine::LoadEngineConfig()
{
    eastl::string ini_file = m_workPath + "RealEngine.ini";

    SI_Error error = m_configIni.LoadFile(ini_file.c_str());
    if (error != SI_OK)
    {
        RE_LOG("Failed to load RealEngine.ini !");
        return;
    }

    m_assetPath = m_workPath + m_configIni.GetValue("RealEngine", "AssetPath");
    m_shaderPath = m_workPath + m_configIni.GetValue("RealEngine", "ShaderPath");
}