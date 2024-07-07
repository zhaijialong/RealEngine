#include "metal_fence.h"
#include "metal_device.h"
#include "utils/log.h"

MetalFence::MetalFence(MetalDevice* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

MetalFence::~MetalFence()
{
    m_pEvent->release();
}

bool MetalFence::Create()
{
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    m_pEvent = device->newSharedEvent();
    if(m_pEvent == nullptr)
    {
        RE_ERROR("[MetalFence] failed to create {}", m_name);
        return false;
    }
    
    SetDebugLabel(m_pEvent, m_name.c_str());
    
    return true;
}

void MetalFence::Wait(uint64_t value)
{
    m_pEvent->waitUntilSignaledValue(value, UINT64_MAX);
}

void MetalFence::Signal(uint64_t value)
{
    m_pEvent->setSignaledValue(value);
}
