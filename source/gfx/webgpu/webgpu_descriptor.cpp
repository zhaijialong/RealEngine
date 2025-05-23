#include "webgpu_descriptor.h"
#include "webgpu_device.h"

WebGPUShaderResourceView::WebGPUShaderResourceView(WebGPUDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

WebGPUShaderResourceView::~WebGPUShaderResourceView()
{
}

bool WebGPUShaderResourceView::Create()
{
    return true;
}

void* WebGPUShaderResourceView::GetHandle() const
{
    return nullptr;
}

uint32_t WebGPUShaderResourceView::GetHeapIndex() const
{
    return 0;
}

WebGPUUnorderedAccessView::WebGPUUnorderedAccessView(WebGPUDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pResource = pResource;
    m_desc = desc;
}

WebGPUUnorderedAccessView::~WebGPUUnorderedAccessView()
{
}

bool WebGPUUnorderedAccessView::Create()
{
    return true;
}

void* WebGPUUnorderedAccessView::GetHandle() const
{
    return nullptr;
}

uint32_t WebGPUUnorderedAccessView::GetHeapIndex() const
{
    return 0;
}

WebGPUConstantBufferView::WebGPUConstantBufferView(WebGPUDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_pBuffer = buffer;
    m_desc = desc;
}

WebGPUConstantBufferView::~WebGPUConstantBufferView()
{
}

bool WebGPUConstantBufferView::Create()
{
    return true;
}

void* WebGPUConstantBufferView::GetHandle() const
{
    return nullptr;
}

uint32_t WebGPUConstantBufferView::GetHeapIndex() const
{
    return 0;
}

WebGPUSampler::WebGPUSampler(WebGPUDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
}

WebGPUSampler::~WebGPUSampler()
{
}

bool WebGPUSampler::Create()
{
    return true;
}

void* WebGPUSampler::GetHandle() const
{
    return nullptr;
}

uint32_t WebGPUSampler::GetHeapIndex() const
{
    return 0;
}
