#include "renderer.h"

void Renderer::CreateDevice()
{
    GfxDeviceDesc desc;
    m_pDevice.reset(CreateGfxDevice(desc));
}
