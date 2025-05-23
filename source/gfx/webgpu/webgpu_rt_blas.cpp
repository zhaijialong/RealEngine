#include "webgpu_rt_blas.h"
#include "webgpu_device.h"

WebGPURayTracingBLAS::WebGPURayTracingBLAS(WebGPUDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPURayTracingBLAS::~WebGPURayTracingBLAS()
{
}

bool WebGPURayTracingBLAS::Create()
{
    return true;
}

void* WebGPURayTracingBLAS::GetHandle() const
{
    return nullptr;
}
