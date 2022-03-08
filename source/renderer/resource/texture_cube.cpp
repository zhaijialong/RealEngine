#include "texture_cube.h"
#include "core/engine.h"
#include "../renderer.h"

TextureCube::TextureCube(const eastl::string& name)
{
    m_name = name;
}

bool TextureCube::Create(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.mip_levels = levels;
    desc.array_size = 6;
    desc.type = GfxTextureType::TextureCube;
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
    srvDesc.type = GfxShaderResourceViewType::TextureCube;
    srvDesc.texture.array_size = 6;

    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    srvDesc.type = GfxShaderResourceViewType::Texture2DArray;

    m_pArraySRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pArraySRV == nullptr)
    {
        return false;
    }

    if (flags & GfxTextureUsageUnorderedAccess)
    {
        GfxUnorderedAccessViewDesc uavDesc;
        uavDesc.type = GfxUnorderedAccessViewType::Texture2DArray;
        uavDesc.texture.array_size = 6;

        for (uint32_t i = 0; i < levels; ++i)
        {
            uavDesc.texture.mip_slice = i;

            IGfxDescriptor* uav = pDevice->CreateUnorderedAccessView(m_pTexture.get(), uavDesc, m_name);
            if (uav == nullptr)
            {
                return false;
            }

            m_pUAV.push_back(eastl::unique_ptr<IGfxDescriptor>(uav));
        }
    }

    return true;
}
