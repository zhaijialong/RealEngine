#include "metal_descriptor.h"
#include "metal_device.h"

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
}

bool MetalSampler::Create()
{
    return true;
}

void* MetalSampler::GetHandle() const
{
    return nullptr;
}

uint32_t MetalSampler::GetHeapIndex() const
{
    return 0;
}
