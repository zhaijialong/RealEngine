#include "metal_swapchain.h"
#include "metal_device.h"

MetalSwapchain::MetalSwapchain(MetalDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalSwapchain::~MetalSwapchain()
{
    m_pView->release();
}

bool MetalSwapchain::Create()
{
    m_pView = (MTK::View*)m_desc.window_handle;
    m_pView->setDevice((MTL::Device*)m_pDevice->GetHandle());
    m_pView->retain();
    
    return true;
}

MTL::Drawable* MetalSwapchain::GetDrawable()
{
    return m_pView->currentDrawable();
}

void MetalSwapchain::AcquireNextBackBuffer()
{
    CA::MetalDrawable* drawable = m_pView->currentDrawable(); // this invokes CAMetalLayer::nextDrawable
    MTL::Texture* texture = drawable->texture();
    
    //todo
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
