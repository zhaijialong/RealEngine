#include "texture_2d.h"
#include "core/engine.h"
#include "../renderer.h"
#include "utils/system.h"

Texture2D::Texture2D(const eastl::string& name)
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

    if (flags & GfxTextureUsageUnorderedAccess)
    {
        GfxUnorderedAccessViewDesc uavDesc;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pTexture.get(), uavDesc, m_name));
        if (m_pUAV == nullptr)
        {
            return false;
        }
    }

    return true;
}
