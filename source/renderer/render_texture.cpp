#include "render_texture.h"
#include "core/engine.h"
#include "renderer.h"
#include "utils/system.h"

RenderTexture::RenderTexture(const std::string& name) : m_resizeConnection({})
{
    m_name = name;
}

RenderTexture::~RenderTexture()
{
    if (m_pWindow)
    {
        Engine::GetInstance()->WindowResizeSignal.disconnect(m_resizeConnection);
    }
}

bool RenderTexture::Create(uint32_t width, uint32_t height, GfxFormat format, GfxTextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.alloc_type = GfxAllocationType::Committed;
    desc.usage = flags;

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));
    if (m_pTexture == nullptr)
    {
        return false;
    }

    if (flags & GfxTextureUsageShaderResource)
    {
        GfxShaderResourceViewDesc srvDesc;
        m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    }

    if (flags & GfxTextureUsageUnorderedAccess)
    {
        //todo : uav
    }

    return true;
}

bool RenderTexture::Create(void* window_handle, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags)
{
    m_pWindow = window_handle;
    m_widthRatio = width_ratio;
    m_heightRatio = height_ratio;
    m_resizeConnection = Engine::GetInstance()->WindowResizeSignal.connect(this, &RenderTexture::OnWindowResize);

    uint32_t width = (uint32_t)ceilf(GetWindowWidth(window_handle) * width_ratio);
    uint32_t height = (uint32_t)ceilf(GetWindowHeight(window_handle) * height_ratio);

    return Create(width, height, format, flags);
}

void RenderTexture::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    if (m_pWindow != window)
    {
        return;
    }

    GfxTextureDesc desc = m_pTexture->GetDesc();
    uint32_t new_width = (uint32_t)ceilf(width * m_widthRatio);
    uint32_t new_height = (uint32_t)ceilf(height * m_heightRatio);

    m_pTexture.reset();
    m_pSRV.reset();
    m_pUAV.reset();

    Create(new_width, new_height, desc.format, desc.usage);
}
