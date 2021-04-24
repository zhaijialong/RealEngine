#pragma once

#include "gfx/gfx.h"
#include <memory>

class Renderer
{
public:
    void CreateDevice();

private:
    std::unique_ptr<IGfxDevice> m_pDevice;
    std::unique_ptr<IGfxSwapchain> m_pSwapchain;
};