#include "metal_texture.h"
#include "metal_device.h"
#include "metal_heap.h"
#include "utils/log.h"
#include "../gfx.h"

MetalTexture::MetalTexture(MetalDevice* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalTexture::~MetalTexture()
{
    ((MetalDevice*)m_pDevice)->Evict(m_pTexture);
    
    if(!m_bSwapchainTexture)
    {
        m_pTexture->release();
    }
}

bool MetalTexture::Create()
{
    MTL::TextureDescriptor* descriptor = ToTextureDescriptor(m_desc);
    
    if(m_desc.heap)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);
        
        MTL::Heap* heap = (MTL::Heap*)m_desc.heap->GetHandle();
        m_pTexture = heap->newTexture(descriptor, m_desc.heap_offset);
    }
    else
    {
        MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
        m_pTexture = device->newTexture(descriptor);
    }
    
    descriptor->release();
    
    if(m_pTexture == nullptr)
    {
        RE_ERROR("[MetalTexture] failed to create {}", m_name);
        return false;
    }
    
    ((MetalDevice*)m_pDevice)->MakeResident(m_pTexture);
    
    SetDebugLabel(m_pTexture, m_name.c_str());
    
    if(m_desc.heap)
    {
        m_pTexture->makeAliasable();
    }
    
    return true;
}

void MetalTexture::SetSwapchainTexture(MTL::Texture* texture)
{
    m_pTexture = texture;
    m_desc.width = (uint32_t)m_pTexture->width();
    m_desc.height = (uint32_t)m_pTexture->height();
    
    m_bSwapchainTexture = true;
}

uint32_t MetalTexture::GetRequiredStagingBufferSize() const
{
    uint32_t size = 0;

    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t min_height = GetFormatBlockHeight(m_desc.format);

    for (uint32_t layer = 0; layer < m_desc.array_size; ++layer)
    {
        for (uint32_t mip = 0; mip < m_desc.mip_levels; ++mip)
        {
            uint32_t width = eastl::max(m_desc.width >> mip, min_width);
            uint32_t height = eastl::max(m_desc.height >> mip, min_height);
            uint32_t depth = eastl::max(m_desc.depth >> mip, 1u);

            size += GetFormatRowPitch(m_desc.format, width) * height * depth;
        }
    }

    return size;
}

uint32_t MetalTexture::GetRowPitch(uint32_t mip_level) const
{
    uint32_t min_width = GetFormatBlockWidth(m_desc.format);
    uint32_t width = eastl::max(m_desc.width >> mip_level, min_width);

    return GetFormatRowPitch(m_desc.format, width) * GetFormatBlockHeight(m_desc.format);
}

GfxTilingDesc MetalTexture::GetTilingDesc() const
{
    //todo
    return GfxTilingDesc();
}

GfxSubresourceTilingDesc MetalTexture::GetTilingDesc(uint32_t subresource) const
{
    //todo
    return GfxSubresourceTilingDesc();
}

void* MetalTexture::GetSharedHandle() const
{
    //todo
    return nullptr;
}
