#include "metal_descriptor.h"
#include "metal_device.h"
#include "metal_texture.h"
#include "utils/log.h"
#include "../gfx.h"

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
    
    MTL::Texture* texture = nullptr;
    MTL::Buffer* buffer = nullptr;
    MTL::PixelFormat format = ToPixelFormat(m_desc.format);
    NS::Range levelRange(0, 1);
    
    if(m_pResource->IsTexture())
    {
        const GfxTextureDesc& textureDesc = ((IGfxTexture*)m_pResource)->GetDesc();
        levelRange = NS::Range(m_desc.texture.mip_slice,
            m_desc.texture.mip_levels == GFX_ALL_SUB_RESOURCE ? textureDesc.mip_levels - m_desc.texture.mip_slice : m_desc.texture.mip_levels);
        
        texture = (MTL::Texture*)m_pResource->GetHandle();
    }
    else
    {
        buffer = (MTL::Buffer*)m_pResource->GetHandle();
    }
    
    switch (m_desc.type)
    {
        case GfxShaderResourceViewType::Texture2D:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType2DArray, levelRange, NS::Range(0, 1));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::Texture2DArray:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType2DArray, levelRange, NS::Range(m_desc.texture.array_slice, m_desc.texture.array_size));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::Texture3D:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType3D, levelRange, NS::Range(0, 1));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::TextureCube:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureTypeCubeArray, levelRange, NS::Range(0, 1));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::TextureCubeArray:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureTypeCubeArray, levelRange, NS::Range(m_desc.texture.array_slice / 6, m_desc.texture.array_size / 6));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxShaderResourceViewType::StructuredBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageStructuredBuffer);
            RE_ASSERT(m_desc.format == GfxFormat::Unknown);
            RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
            RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);
            
            IRDescriptorTableSetBuffer(descriptorTableEntry, buffer->gpuAddress(), 0);
            break;
        }
        case GfxShaderResourceViewType::TypedBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageTypedBuffer);
            RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
            RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);
            
            uint32_t element_num = m_desc.buffer.size / bufferDesc.stride;
            
            MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->textureBufferDescriptor(format, element_num, buffer->resourceOptions(), MTL::TextureUsageShaderRead);
            m_pTextureView = buffer->newTexture(descriptor, m_desc.buffer.offset, GetFormatRowPitch(m_desc.format, element_num));
            descriptor->release();
            
            IRBufferView bufferView = {};
            bufferView.buffer = buffer;
            bufferView.bufferOffset = m_desc.buffer.offset;
            bufferView.bufferSize = m_desc.buffer.size;
            bufferView.textureBufferView = m_pTextureView;
            bufferView.typedBuffer = true;
            IRDescriptorTableSetBufferView(descriptorTableEntry, &bufferView);
            break;
        }
        case GfxShaderResourceViewType::RawBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageRawBuffer);
            RE_ASSERT(bufferDesc.stride % 4 == 0);
            RE_ASSERT(m_desc.buffer.offset % 4 == 0);
            RE_ASSERT(m_desc.buffer.size % 4 == 0);
            
            IRDescriptorTableSetBuffer(descriptorTableEntry, buffer->gpuAddress(), 0);
            break;
        }
        case GfxShaderResourceViewType::RayTracingTLAS:
        {
            //todo
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
    
    MTL::Texture* texture = nullptr;
    MTL::Buffer* buffer = nullptr;
    MTL::PixelFormat format = ToPixelFormat(m_desc.format);
    NS::Range levelRange(m_desc.texture.mip_slice, 1);
    
    if(m_pResource->IsTexture())
    {
        texture = (MTL::Texture*)m_pResource->GetHandle();
    }
    else
    {
        buffer = (MTL::Buffer*)m_pResource->GetHandle();
    }
    
    switch (m_desc.type)
    {
        case GfxUnorderedAccessViewType::Texture2D:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType2DArray, levelRange, NS::Range(0, 1));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxUnorderedAccessViewType::Texture2DArray:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType2DArray, levelRange, NS::Range(m_desc.texture.array_slice, m_desc.texture.array_size));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxUnorderedAccessViewType::Texture3D:
        {
            m_pTextureView = texture->newTextureView(format, MTL::TextureType3D, levelRange, NS::Range(0, 1));
            IRDescriptorTableSetTexture(descriptorTableEntry, m_pTextureView, 0.0f, 0);
            break;
        }
        case GfxUnorderedAccessViewType::StructuredBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageStructuredBuffer);
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
            RE_ASSERT(m_desc.format == GfxFormat::Unknown);
            RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
            RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);
            
            IRDescriptorTableSetBuffer(descriptorTableEntry, buffer->gpuAddress(), 0);
            break;
        }
        case GfxUnorderedAccessViewType::TypedBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageTypedBuffer);
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
            RE_ASSERT(m_desc.buffer.offset % bufferDesc.stride == 0);
            RE_ASSERT(m_desc.buffer.size % bufferDesc.stride == 0);
            
            uint32_t element_num = m_desc.buffer.size / bufferDesc.stride;
            
            MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->textureBufferDescriptor(format, element_num, buffer->resourceOptions(), MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite); // todo MTL::TextureUsageShaderAtomic
            m_pTextureView = buffer->newTexture(descriptor, m_desc.buffer.offset, GetFormatRowPitch(m_desc.format, element_num));
            descriptor->release();
            
            IRBufferView bufferView = {};
            bufferView.buffer = buffer;
            bufferView.bufferOffset = m_desc.buffer.offset;
            bufferView.bufferSize = m_desc.buffer.size;
            bufferView.textureBufferView = m_pTextureView;
            bufferView.typedBuffer = true;
            IRDescriptorTableSetBufferView(descriptorTableEntry, &bufferView);
            break;
        }
        case GfxUnorderedAccessViewType::RawBuffer:
        {
            const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)m_pResource)->GetDesc();
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageRawBuffer);
            RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);
            RE_ASSERT(bufferDesc.stride % 4 == 0);
            RE_ASSERT(m_desc.buffer.offset % 4 == 0);
            RE_ASSERT(m_desc.buffer.size % 4 == 0);
            
            IRDescriptorTableSetBuffer(descriptorTableEntry, buffer->gpuAddress(), 0);
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
