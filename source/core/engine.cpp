#include "engine.h"

Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

void Engine::Init(void* window_handle, uint32_t window_width, uint32_t window_height)
{
    m_pRenderer = std::make_unique<Renderer>();
    m_pRenderer->CreateDevice();
}

void Engine::Shut()
{
}

void Engine::Frame()
{
}
