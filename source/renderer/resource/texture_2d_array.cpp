#include "texture_2d_array.h"
#include "core/engine.h"
#include "../renderer.h"

Texture2DArray::Texture2DArray(const eastl::string& name)
{
    m_name = name;
}

bool Texture2DArray::Create(uint32_t width, uint32_t height, uint32_t levels, uint32_t array_size, GfxFormat format, GfxTextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.mip_levels = levels;
    desc.array_size = array_size;
    desc.type = GfxTextureType::Texture2DArray;
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
    srvDesc.type = GfxShaderResourceViewType::Texture2DArray;
    srvDesc.format = format;
    srvDesc.texture.mip_levels = levels;
    srvDesc.texture.array_size = array_size;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (flags & GfxTextureUsageUnorderedAccess)
    {
        //todo UAV
    }

    return true;
}
