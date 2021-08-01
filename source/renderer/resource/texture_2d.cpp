#include "texture_2d.h"
#include "core/engine.h"
#include "../renderer.h"
#include "utils/system.h"

#include "stb/stb_image.h"

Texture2D::Texture2D(const std::string& name) : m_resizeConnection({})
{
    m_name = name;
}

bool Texture2D::Create(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.mip_levels = levels;
    desc.format = format;
    desc.usage = flags;

    if ((flags & GfxTextureUsageRenderTarget) || (flags & GfxTextureUsageDepthStencil) || (flags & GfxTextureUsageUnorderedAccess))
    {
        desc.alloc_type = GfxAllocationType::Committed;
    }

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));
    if (m_pTexture == nullptr)
    {
        return false;
    }

    GfxShaderResourceViewDesc srvDesc;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    return true;
}

bool Texture2D::Create(void* window_handle, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags)
{
    m_pWindow = window_handle;
    m_widthRatio = width_ratio;
    m_heightRatio = height_ratio;
    m_resizeConnection = Engine::GetInstance()->WindowResizeSignal.connect(this, &Texture2D::OnWindowResize);

    uint32_t width = (uint32_t)ceilf(GetWindowWidth(window_handle) * width_ratio);
    uint32_t height = (uint32_t)ceilf(GetWindowHeight(window_handle) * height_ratio);

    return Create(width, height, 1, format, flags);
}

void Texture2D::OnWindowResize(void* window, uint32_t width, uint32_t height)
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

    Create(new_width, new_height, 1, desc.format, desc.usage);
}
