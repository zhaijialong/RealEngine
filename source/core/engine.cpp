#include "engine.h"

Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

void Engine::Init(void* window_handle, uint32_t window_width, uint32_t window_height)
{
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
