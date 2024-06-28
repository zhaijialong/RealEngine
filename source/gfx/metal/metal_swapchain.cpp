#include "metal_swapchain.h"
#include "metal_device.h"

MetalSwapchain::MetalSwapchain(MetalDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    
}

MetalSwapchain::~MetalSwapchain()
{
    
}

bool MetalSwapchain::Create()
{
    return true;
}

void MetalSwapchain::Present()
{
    
}

void* MetalSwapchain::GetHandle() const
{
    return nullptr;
}

void MetalSwapchain::AcquireNextBackBuffer()
{
    
}

IGfxTexture* MetalSwapchain::GetBackBuffer() const
{
    return nullptr;
}

bool MetalSwapchain::Resize(uint32_t width, uint32_t height)
{
    return true;
}

void MetalSwapchain::SetVSyncEnabled(bool value)
{
}
