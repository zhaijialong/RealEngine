#include "d3d12_rt_blas.h"
#include "d3d12_device.h"

D3D12RayTracingBLAS::D3D12RayTracingBLAS(D3D12Device* pDevice, const GfxRayTracingBLASDesc& desc, const std::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

bool D3D12RayTracingBLAS::Create()
{
    return false;
}
