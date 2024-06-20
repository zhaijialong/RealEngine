#include "vulkan_rt_tlas.h"
#include "vulkan_device.h"

VulkanRayTracingTLAS::VulkanRayTracingTLAS(VulkanDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanRayTracingTLAS::~VulkanRayTracingTLAS()
{
}
