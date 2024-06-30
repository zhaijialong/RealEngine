#include "metal_texture.h"
#include "metal_device.h"
#include "metal_heap.h"
#include "metal_utils.h"
#include "utils/assert.h"
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
    m_pTexture->release();
}

bool MetalTexture::Create()
{
    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setWidth(m_desc.width);
    descriptor->setHeight(m_desc.height);
    descriptor->setDepth(m_desc.depth);
    descriptor->setMipmapLevelCount(m_desc.mip_levels);
    if(m_desc.type == GfxTextureType::TextureCube || m_desc.type == GfxTextureType::TextureCubeArray)
    {
        RE_ASSERT(m_desc.array_size % 6 == 0);
        descriptor->setArrayLength(m_desc.array_size / 6);
    }
    else
    {
        descriptor->setArrayLength(m_desc.array_size);
    }
    descriptor->setTextureType(ToTextureType(m_desc.type));
    descriptor->setPixelFormat(ToPixelFormat(m_desc.format));
    descriptor->setResourceOptions(ToResourceOptions(m_desc.memory_type));
    descriptor->setUsage(ToTextureUsage(m_desc.usage));
    
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
    
    SetDebugLabel(m_pTexture, m_name.c_str());
    
    return true;
}

uint32_t MetalTexture::GetRequiredStagingBufferSize() const
{
    return m_pDevice->GetAllocationSize(m_desc);
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
