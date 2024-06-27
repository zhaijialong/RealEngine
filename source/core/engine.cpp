#include "engine.h"
#include "utils/log.h"
#include "utils/profiler.h"
#include "utils/system.h"
#include "enkiTS/TaskScheduler.h"
#include "rpmalloc/rpmalloc.h"
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "magic_enum/magic_enum.hpp"
#define SOKOL_IMPL
#include "sokol/sokol_time.h"
#include "imgui/imgui.h"
#include "simpleini/SimpleIni.h"

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
#if RE_PLATFORM_WINDOWS
    auto console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
    auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
#endif
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((work_path + "log.txt").c_str(), true);
    auto logger = std::make_shared<spdlog::logger>("RealEngine", spdlog::sinks_init_list{ console_sink, file_sink });
    
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [thread %t] %v");
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_every(std::chrono::milliseconds(10));

    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
    MicroProfileOnThreadCreate("Main Thread");

    enki::TaskSchedulerConfig tsConfig;
    tsConfig.profilerCallbacks.threadStart = [](uint32_t i)
    {
        rpmalloc_thread_initialize();

        eastl::string thread_name = fmt::format("Worker Thread {}", i).c_str();
        MicroProfileOnThreadCreate(thread_name.c_str());
        SetCurrentThreadName(thread_name);
    };
    tsConfig.profilerCallbacks.threadStop = [](uint32_t)
    {
        MicroProfileOnThreadExit();
        rpmalloc_thread_finalize(1); 
    };
    tsConfig.customAllocator.alloc = [](size_t align, size_t size, void* userData, const char* file, int line)
    {
        return RE_ALLOC(size, align);
    };
    tsConfig.customAllocator.free = [](void* ptr, size_t size, void* userData, const char* file, int line)
    {
        RE_FREE(ptr);
    };

    m_pTaskScheduler.reset(new enki::TaskScheduler());
    m_pTaskScheduler->Initialize(tsConfig);

    m_windowHandle = window_handle;
    m_workPath = work_path;
    
    eastl::string ini_file = m_workPath + "RealEngine.ini";
    
    CSimpleIniA configIni;
    if (configIni.LoadFile(ini_file.c_str()) != SI_OK)
    {
        RE_ERROR("Failed to load RealEngine.ini !");
    }

    m_assetPath = m_workPath + configIni.GetValue("RealEngine", "AssetPath");
    m_shaderPath = m_workPath + configIni.GetValue("RealEngine", "ShaderPath");

    const char* backend = configIni.GetValue("Render", "Backend");
    GfxRenderBackend renderBackend = magic_enum::enum_cast<GfxRenderBackend>(backend).
#if RE_PLATFORM_WINDOWS
        value_or(GfxRenderBackend::D3D12);
#else
        value_or(GfxRenderBackend::Metal);
#endif

    m_pRenderer = eastl::make_unique<Renderer>();
    m_pRenderer->SetAsyncComputeEnabled(configIni.GetBoolValue("Render", "AsyncCompute"));
    if (!m_pRenderer->CreateDevice(renderBackend, window_handle, window_width, window_height))
    {
        exit(0);
    }

    m_pWorld = eastl::make_unique<World>();
    m_pWorld->LoadScene(m_assetPath + configIni.GetValue("World", "Scene"));

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
    m_pTaskScheduler.reset();

    MicroProfileShutdown();
    m_pRenderer.reset();

    spdlog::shutdown();
}

void Engine::Tick()
{
    CPU_EVENT("Tick", "Engine::Tick");

    m_frameTime = (float)stm_sec(stm_laptime(&m_lastFrameTime));

    m_pGUI->Tick();

    ImGuiIO& io = ImGui::GetIO();
    bool isMinimized = (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f);

    if (isMinimized)
    {
        ImGui::Render(); //this has to be called every frame
    }
    else
    {
        m_pEditor->Tick();
        m_pWorld->Tick(m_frameTime);

        m_pRenderer->RenderFrame();
    }

    MicroProfileFlip(0);
}
