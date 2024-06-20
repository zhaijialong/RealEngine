#include "mock_heap.h"
#include "mock_device.h"

MockHeap::MockHeap(MockDevice* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockHeap::~MockHeap()
{
}

bool MockHeap::Create()
{
    return true;
}

void* MockHeap::GetHandle() const
{
    return nullptr;
}
