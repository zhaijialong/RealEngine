#include "webgpu_heap.h"
#include "webgpu_device.h"

WebGPUHeap::WebGPUHeap(WebGPUDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPUHeap::~WebGPUHeap()
{
}

bool WebGPUHeap::Create()
{
    return true;
}

void* WebGPUHeap::GetHandle() const
{
    return nullptr;
}
