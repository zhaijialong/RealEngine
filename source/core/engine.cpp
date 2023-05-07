#include "engine.h"
#include "utils/log.h"
#include "utils/profiler.h"
#include "utils/system.h"
#include "enkiTS/TaskScheduler.h"
#include "rpmalloc/rpmalloc.h"
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/basic_file_sink.h"

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
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((work_path + "log.txt").c_str(), true);
    auto logger = std::make_shared<spdlog::logger>("RealEngine", spdlog::sinks_init_list{ msvc_sink, file_sink });
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [thread %t] %v");
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_every(std::chrono::milliseconds(10));

    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
    MicroProfileOnThreadCreate("Main Thread");

    enki::TaskSchedulerConfig config;
    config.profilerCallbacks.threadStart = [](uint32_t i) 
    { 
        rpmalloc_thread_initialize();

        eastl::string thread_name = fmt::format("Worker Thread {}", i).c_str();
        MicroProfileOnThreadCreate(thread_name.c_str());
        SetCurrentThreadName(thread_name);
    };
    config.profilerCallbacks.threadStop = [](uint32_t) 
    { 
        MicroProfileOnThreadExit();
        rpmalloc_thread_finalize(1); 
    };

    m_pTaskScheduler.reset(new enki::TaskScheduler());
    m_pTaskScheduler->Initialize(config);

    m_windowHandle = window_handle;
    m_workPath = work_path;
    LoadEngineConfig();

    m_pRenderer = eastl::make_unique<Renderer>();
    m_pRenderer->CreateDevice(window_handle, window_width, window_height);
    m_pRenderer->SetAsyncComputeEnabled(m_configIni.GetBoolValue("Render", "AsyncCompute"));

    m_pWorld = eastl::make_unique<World>();
    m_pWorld->LoadScene(m_assetPath + m_configIni.GetValue("World", "Scene"));

    m_pGUI = eastl::make_unique<GUI>();
    m_pGUI->Init();

    m_pEditor = eastl::make_unique<Editor>();

    stm_setup();
}

void Engine::Shut()
{
    m_pTaskScheduler->WaitforAll();

    m_pWorld.reset();
    m_pEditor.reset();
    m_pGUI.reset();
    m_pRenderer.reset();
    m_pTaskScheduler.reset();

    MicroProfileShutdown();
    spdlog::shutdown();
}

void Engine::Tick()
{
    CPU_EVENT("Tick", "Engine::Tick");

    m_frameTime = (float)stm_sec(stm_laptime(&m_lastFrameTime));

    m_pGUI->Tick();
    m_pEditor->Tick();
    m_pWorld->Tick(m_frameTime);

    m_pRenderer->RenderFrame();

    MicroProfileFlip(0);
}

void Engine::LoadEngineConfig()
{
    eastl::string ini_file = m_workPath + "RealEngine.ini";

    SI_Error error = m_configIni.LoadFile(ini_file.c_str());
    if (error != SI_OK)
    {
        RE_ERROR("Failed to load RealEngine.ini !");
        return;
    }

    m_assetPath = m_workPath + m_configIni.GetValue("RealEngine", "AssetPath");
    m_shaderPath = m_workPath + m_configIni.GetValue("RealEngine", "ShaderPath");
}