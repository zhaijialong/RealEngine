#include "metal_descriptor.h"
#include "metal_device.h"
#include "metal_texture.h"
#include "utils/log.h"

MetalShaderResourceView::MetalShaderResourceView(MetalDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

MetalShaderResourceView::~MetalShaderResourceView()
{
    if(m_pTextureView)
    {
        m_pTextureView->release();
    }
    
    ((MetalDevice*)m_pDevice)->FreeResourceDescriptor(m_heapIndex);
}

bool MetalShaderResourceView::Create()
{
    IRDescriptorTableEntry* descriptorTableEntry = nullptr;
    m_heapIndex = ((MetalDevice*)m_pDevice)->AllocateResourceDescriptor(&descriptorTableEntry);
    
    switch (m_desc.type)
    {
        case GfxShaderResourceViewType::Texture2D:
        {
            const GfxTextureDesc& textureDesc = ((IGfxTexture*)m_pResource)->GetDesc();
            NS::Range levelRange = m_desc.texture.mip_levels == GFX_ALL_SUB_RESOURCE ?
                NS::Range(m_desc.texture.mip_slice, textureDesc.mip_levels - m_desc.texture.mip_slice) :
                NS::Range(m_desc.texture.mip_slice, m_desc.texture.mip_levels);
            
            MTL::Texture* texture = (MTL::Texture*)m_pResource->GetHandle();
            m_pTextureView = texture->newTextureView(ToPixelFormat(m_desc.format), MTL::TextureType2DArray, levelRange, NS::Range(0, 1));
            
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::Texture2DArray:
        {
            
            break;
        }
        case GfxShaderResourceViewType::Texture3D:
        {
            IRDescriptorTableSetTexture(descriptorTableEntry, (MTL::Texture*)m_pResource->GetHandle(), 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::TextureCube:
        {
            
            break;
        }
        case GfxShaderResourceViewType::TextureCubeArray:
        {
            
            break;
        }
        case GfxShaderResourceViewType::StructuredBuffer:
        {
            
            break;
        }
        case GfxShaderResourceViewType::TypedBuffer:
        {
            
            break;
        }
        case GfxShaderResourceViewType::RawBuffer:
        {
            
            break;
        }
        case GfxShaderResourceViewType::RayTracingTLAS:
        {
            
            break;
        }
        default:
            break;
    }
    
    return true;
}

MetalUnorderedAccessView::MetalUnorderedAccessView(MetalDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

MetalUnorderedAccessView::~MetalUnorderedAccessView()
{
    if(m_pTextureView)
    {
        m_pTextureView->release();
    }
    
    ((MetalDevice*)m_pDevice)->FreeResourceDescriptor(m_heapIndex);
}

bool MetalUnorderedAccessView::Create()
{
    IRDescriptorTableEntry* descriptorTableEntry = nullptr;
    m_heapIndex = ((MetalDevice*)m_pDevice)->AllocateResourceDescriptor(&descriptorTableEntry);
    
    switch (m_desc.type)
    {
        case GfxUnorderedAccessViewType::Texture2D:
        {
            
            break;
        }
        case GfxUnorderedAccessViewType::Texture2DArray:
        {
            
            break;
        }
        case GfxUnorderedAccessViewType::Texture3D:
        {
            
            break;
        }
        case GfxUnorderedAccessViewType::StructuredBuffer:
        {
            
            break;
        }
        case GfxUnorderedAccessViewType::TypedBuffer:
        {
            
            break;
        }
        case GfxUnorderedAccessViewType::RawBuffer:
        {
            
            break;
        }
        default:
            break;
    }
    
    return true;
}

MetalConstantBufferView::MetalConstantBufferView(MetalDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pBuffer = buffer;
    m_desc = desc;
}

MetalConstantBufferView::~MetalConstantBufferView()
{
    ((MetalDevice*)m_pDevice)->FreeResourceDescriptor(m_heapIndex);
}

bool MetalConstantBufferView::Create()
{
    IRDescriptorTableEntry* descriptorTableEntry = nullptr;
    m_heapIndex = ((MetalDevice*)m_pDevice)->AllocateResourceDescriptor(&descriptorTableEntry);
    IRDescriptorTableSetBuffer(descriptorTableEntry, m_pBuffer->GetGpuAddress(), 0);
    
    return true;
}

MetalSampler::MetalSampler(MetalDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
}

MetalSampler::~MetalSampler()
{
    m_pSampler->release();
    
    ((MetalDevice*)m_pDevice)->FreeSamplerDescriptor(m_heapIndex);
}

bool MetalSampler::Create()
{
    MTL::SamplerDescriptor* descritor = MTL::SamplerDescriptor::alloc()->init();
    descritor->setMinFilter(ToSamplerMinMagFilter(m_desc.min_filter));
    descritor->setMagFilter(ToSamplerMinMagFilter(m_desc.mag_filter));
    descritor->setMipFilter(ToSamplerMipFilter(m_desc.mip_filter));
    descritor->setMaxAnisotropy((NS::UInteger)m_desc.max_anisotropy);
    descritor->setSAddressMode(ToSamplerAddressMode(m_desc.address_u));
    descritor->setTAddressMode(ToSamplerAddressMode(m_desc.address_v));
    descritor->setRAddressMode(ToSamplerAddressMode(m_desc.address_w));
    descritor->setNormalizedCoordinates(true);
    descritor->setLodMinClamp(m_desc.min_lod);
    descritor->setLodMaxClamp(m_desc.max_lod);
    descritor->setSupportArgumentBuffers(true);
    
    // unfortunately, Metal does not support Min/Max reduction mode
    
    if(m_desc.reduction_mode == GfxSamplerReductionMode::Compare)
    {
        descritor->setCompareFunction(ToCompareFunction(m_desc.compare_func));
    }
    
    if(m_desc.border_color[0] == 0.0f && m_desc.border_color[1] == 0.0f && m_desc.border_color[2] == 0.0f && m_desc.border_color[2] == 0.0f)
    {
        descritor->setBorderColor(MTL::SamplerBorderColorTransparentBlack);
    }
    else if(m_desc.border_color[0] == 0.0f && m_desc.border_color[1] == 0.0f && m_desc.border_color[2] == 0.0f && m_desc.border_color[2] == 1.0f)
    {
        descritor->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
    }
    else if(m_desc.border_color[0] == 1.0f && m_desc.border_color[1] == 1.0f && m_desc.border_color[2] == 1.0f && m_desc.border_color[2] == 1.0f)
    {
        descritor->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
    }
    else
    {
        RE_ASSERT(false); //unsupported border color
    }
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    m_pSampler = device->newSamplerState(descritor);
    descritor->release();
    
    if(m_pSampler == nullptr)
    {
        RE_ERROR("[MetalSampler] failed to create {}", m_name);
        return false;
    }
    
    IRDescriptorTableEntry* descriptorTableEntry = nullptr;
    m_heapIndex = ((MetalDevice*)m_pDevice)->AllocateSamplerDescriptor(&descriptorTableEntry);
    IRDescriptorTableSetSampler(descriptorTableEntry, m_pSampler, m_desc.mip_bias);
    
    return true;
}
