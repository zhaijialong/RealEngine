#include "mock_rt_tlas.h"
#include "mock_device.h"

MockRayTracingTLAS::MockRayTracingTLAS(MockDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockRayTracingTLAS::~MockRayTracingTLAS()
{
}

bool MockRayTracingTLAS::Create()
{
    return true;
}

void* MockRayTracingTLAS::GetHandle() const
{
    return nullptr;
}
