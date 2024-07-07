#include "metal_heap.h"
#include "metal_device.h"
#include "utils/log.h"

MetalHeap::MetalHeap(MetalDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalHeap::~MetalHeap()
{
    m_pHeap->release();
}

bool MetalHeap::Create()
{
    RE_ASSERT(m_desc.size % (64 * 1024) == 0);
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    MTL::HeapDescriptor* descriptor = MTL::HeapDescriptor::alloc()->init();
    descriptor->setSize(m_desc.size);
    descriptor->setResourceOptions(ToResourceOptions(m_desc.memory_type));
    descriptor->setType(MTL::HeapTypePlacement); //TODO : MTL::HeapTypeSparse for sparse textures
    
    m_pHeap = device->newHeap(descriptor);
    descriptor->release();
    
    if (m_pHeap == nullptr)
    {
        RE_ERROR("[MetalHeap] failed to create {}", m_name);
        return false;
    }
    
    SetDebugLabel(m_pHeap, m_name.c_str());
    
    return true;
}
