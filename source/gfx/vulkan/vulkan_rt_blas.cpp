#include "vulkan_rt_blas.h"
#include "vulkan_device.h"

VulkanRayTracingBLAS::VulkanRayTracingBLAS(VulkanDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanRayTracingBLAS::~VulkanRayTracingBLAS()
{
}
