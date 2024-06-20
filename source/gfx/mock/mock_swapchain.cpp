#include "mock_swapchain.h"
#include "mock_device.h"
#include "mock_texture.h"
#include "utils/fmt.h"

MockSwapchain::MockSwapchain(MockDevice* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockSwapchain::~MockSwapchain()
{
    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();
}

bool MockSwapchain::Create()
{
    return CreateTextures();
}

void MockSwapchain::Present()
{
}

void* MockSwapchain::GetHandle() const
{
    return nullptr;
}

void MockSwapchain::AcquireNextBackBuffer()
{
    m_currentBackBuffer = (m_currentBackBuffer + 1) % m_desc.backbuffer_count;
}

IGfxTexture* MockSwapchain::GetBackBuffer() const
{
    return m_backBuffers[m_currentBackBuffer];
}

bool MockSwapchain::Resize(uint32_t width, uint32_t height)
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

void MockSwapchain::SetVSyncEnabled(bool value)
{
}

bool MockSwapchain::CreateTextures()
{
    GfxTextureDesc textureDesc;
    textureDesc.width = m_desc.width;
    textureDesc.height = m_desc.height;
    textureDesc.format = m_desc.backbuffer_format;
    textureDesc.usage = GfxTextureUsageRenderTarget;

    for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
    {
        eastl::string name = fmt::format("{} texture {}", m_name, i).c_str();

        MockTexture* texture = new MockTexture((MockDevice*)m_pDevice, textureDesc, name);
        m_backBuffers.push_back(texture);
    }

    return true;
}
