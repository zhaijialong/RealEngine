#include "mock_fence.h"
#include "mock_device.h"

MockFence::MockFence(MockDevice* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

MockFence::~MockFence()
{
}

bool MockFence::Create()
{
    return true;
}

void* MockFence::GetHandle() const
{
    return nullptr;
}

void MockFence::Wait(uint64_t value)
{
}

void MockFence::Signal(uint64_t value)
{
}
