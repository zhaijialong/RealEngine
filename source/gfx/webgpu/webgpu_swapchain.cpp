#include "webgpu_swapchain.h"
#include "webgpu_device.h"
#include "webgpu_texture.h"
#include "utils/fmt.h"

WebGPUSwapchain::WebGPUSwapchain(WebGPUDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPUSwapchain::~WebGPUSwapchain()
{
    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();
}

bool WebGPUSwapchain::Create()
{
    return CreateTextures();
}

void WebGPUSwapchain::Present()
{
}

void* WebGPUSwapchain::GetHandle() const
{
    return nullptr;
}

void WebGPUSwapchain::AcquireNextBackBuffer()
{
    m_currentBackBuffer = (m_currentBackBuffer + 1) % m_desc.backbuffer_count;
}

IGfxTexture* WebGPUSwapchain::GetBackBuffer() const
{
    return m_backBuffers[m_currentBackBuffer];
}

bool WebGPUSwapchain::Resize(uint32_t width, uint32_t height)
{
    if (m_desc.width == width && m_desc.height == height)
    {
        return false;
    }

    m_desc.width = width;
    m_desc.height = height;
    m_currentBackBuffer = 0;

    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();

    return CreateTextures();
}

void WebGPUSwapchain::SetVSyncEnabled(bool value)
{
}

bool WebGPUSwapchain::CreateTextures()
{
    GfxTextureDesc textureDesc;
    textureDesc.width = m_desc.width;
    textureDesc.height = m_desc.height;
    textureDesc.format = m_desc.backbuffer_format;
    textureDesc.usage = GfxTextureUsageRenderTarget;

    for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
    {
        eastl::string name = fmt::format("{} texture {}", m_name, i).c_str();

        WebGPUTexture* texture = new WebGPUTexture((WebGPUDevice*)m_pDevice, textureDesc, name);
        m_backBuffers.push_back(texture);
    }

    return true;
}
