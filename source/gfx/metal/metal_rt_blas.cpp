#include "metal_rt_blas.h"
#include "metal_device.h"

MetalRayTracingBLAS::MetalRayTracingBLAS(MetalDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalRayTracingBLAS::~MetalRayTracingBLAS()
{
}

bool MetalRayTracingBLAS::Create()
{
    return true;
}

void* MetalRayTracingBLAS::GetHandle() const
{
    return nullptr;
}
