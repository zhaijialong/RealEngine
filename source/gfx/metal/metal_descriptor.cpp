#include "metal_descriptor.h"
#include "metal_device.h"
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
}

bool MetalShaderResourceView::Create()
{
    return true;
}

void* MetalShaderResourceView::GetHandle() const
{
    return nullptr;
}

uint32_t MetalShaderResourceView::GetHeapIndex() const
{
    return 0;
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
}

bool MetalUnorderedAccessView::Create()
{
    return true;
}

void* MetalUnorderedAccessView::GetHandle() const
{
    return nullptr;
}

uint32_t MetalUnorderedAccessView::GetHeapIndex() const
{
    return 0;
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
}

bool MetalConstantBufferView::Create()
{
    return true;
}

void* MetalConstantBufferView::GetHandle() const
{
    return nullptr;
}

uint32_t MetalConstantBufferView::GetHeapIndex() const
{
    return 0;
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
