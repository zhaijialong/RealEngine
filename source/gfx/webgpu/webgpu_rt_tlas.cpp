#include "webgpu_rt_tlas.h"
#include "webgpu_device.h"

WebGPURayTracingTLAS::WebGPURayTracingTLAS(WebGPUDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

WebGPURayTracingTLAS::~WebGPURayTracingTLAS()
{
}

bool WebGPURayTracingTLAS::Create()
{
    return true;
}

void* WebGPURayTracingTLAS::GetHandle() const
{
    return nullptr;
}
