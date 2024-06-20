#include "mock_rt_blas.h"
#include "mock_device.h"

MockRayTracingBLAS::MockRayTracingBLAS(MockDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MockRayTracingBLAS::~MockRayTracingBLAS()
{
}

bool MockRayTracingBLAS::Create()
{
    return true;
}

void* MockRayTracingBLAS::GetHandle() const
{
    return nullptr;
}
