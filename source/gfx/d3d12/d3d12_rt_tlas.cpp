#include "d3d12_rt_tlas.h"
#include "d3d12_device.h"

D3D12RayTracingTLAS::D3D12RayTracingTLAS(D3D12Device* pDevice, const GfxRayTracingTLASDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

bool D3D12RayTracingTLAS::Create()
{
    return false;
}
