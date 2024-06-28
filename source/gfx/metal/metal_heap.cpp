#include "metal_heap.h"
#include "metal_device.h"

MetalHeap::MetalHeap(MetalDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalHeap::~MetalHeap()
{
}

bool MetalHeap::Create()
{
    return true;
}

void* MetalHeap::GetHandle() const
{
    return nullptr;
}
