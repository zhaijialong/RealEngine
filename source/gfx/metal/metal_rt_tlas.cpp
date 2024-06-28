#include "metal_rt_tlas.h"
#include "metal_device.h"

MetalRayTracingTLAS::MetalRayTracingTLAS(MetalDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalRayTracingTLAS::~MetalRayTracingTLAS()
{
}

bool MetalRayTracingTLAS::Create()
{
    return true;
}

void* MetalRayTracingTLAS::GetHandle() const
{
    return nullptr;
}
