#pragma once

#include "world.h"
#include "renderer/renderer.h"

#include <memory>

class Engine
{
public:
    static Engine* GetInstance();

    void Init(void* window_handle, uint32_t window_width, uint32_t window_height);
    void Shut();

    void Frame();

private:
    std::unique_ptr<World> m_pWorld;
    std::unique_ptr<Renderer> m_pRenderer;
};