#include "metal_fence.h"
#include "metal_device.h"

MetalFence::MetalFence(MetalDevice* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

MetalFence::~MetalFence()
{
}

bool MetalFence::Create()
{
    return true;
}

void* MetalFence::GetHandle() const
{
    return nullptr;
}

void MetalFence::Wait(uint64_t value)
{
}

void MetalFence::Signal(uint64_t value)
{
}
