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
    VulkanDevice* device = (VulkanDevice*)m_pDevice;
    device->Delete(m_accelerationStructure);
}

bool VulkanRayTracingTLAS::Create()
{
    return false;
}

VkDeviceAddress VulkanRayTracingTLAS::GetGpuAddress() const
{
    VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = m_accelerationStructure;

    return vkGetAccelerationStructureDeviceAddressKHR((VkDevice)m_pDevice->GetHandle(), &info);
}
