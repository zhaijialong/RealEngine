#include "vulkan_rt_tlas.h"
#include "vulkan_device.h"
#include "vulkan_rt_blas.h"
#include "utils/log.h"

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
    device->Delete(m_asBuffer);
    device->Delete(m_asBufferAllocation);
    device->Delete(m_scratchBuffer);
    device->Delete(m_scratchBufferAllocation);
    device->Delete(m_instanceBuffer);
    device->Delete(m_instanceBufferAllocation);
}

bool VulkanRayTracingTLAS::Create()
{
    VkDevice device = (VkDevice)m_pDevice->GetHandle();

    VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = ToVulkanAccelerationStructureFlags(m_desc.flags);
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &m_desc.instance_count, &sizeInfo);

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = sizeInfo.accelerationStructureSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &m_asBuffer, &m_asBufferAllocation, nullptr);

    bufferInfo.size = sizeInfo.buildScratchSize;
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &m_scratchBuffer, &m_scratchBufferAllocation, nullptr);

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = m_asBuffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkResult result = vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_accelerationStructure);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanRayTracingTLAS] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, m_accelerationStructure, m_name.c_str());
    SetDebugName(device, VK_OBJECT_TYPE_BUFFER, m_asBuffer, m_name.c_str());
    vmaSetAllocationName(allocator, m_asBufferAllocation, m_name.c_str());

    m_instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * m_desc.instance_count * GFX_MAX_INFLIGHT_FRAMES;

    allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    bufferInfo.size = m_instanceBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VmaAllocationInfo info;
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &m_instanceBuffer, &m_instanceBufferAllocation, &info);
    m_instanceBufferCpuAddress = info.pMappedData;

    VkBufferDeviceAddressInfo addressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addressInfo.buffer = m_instanceBuffer;
    m_instanceBufferGpuAddress = vkGetBufferDeviceAddress((VkDevice)m_pDevice->GetHandle(), &addressInfo);

    return true;
}

VkDeviceAddress VulkanRayTracingTLAS::GetGpuAddress() const
{
    VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = m_accelerationStructure;

    return vkGetAccelerationStructureDeviceAddressKHR((VkDevice)m_pDevice->GetHandle(), &info);
}

void VulkanRayTracingTLAS::GetBuildInfo(VkAccelerationStructureBuildGeometryInfoKHR& info, VkAccelerationStructureGeometryKHR& geometry,
    const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    RE_ASSERT(instance_count <= m_desc.instance_count);

    if (m_currentInstanceBufferOffset + sizeof(VkAccelerationStructureInstanceKHR) * instance_count > m_instanceBufferSize)
    {
        m_currentInstanceBufferOffset = 0;
    }

    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = m_instanceBufferGpuAddress + m_currentInstanceBufferOffset;

    info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    info.flags = ToVulkanAccelerationStructureFlags(m_desc.flags);
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.dstAccelerationStructure = m_accelerationStructure;
    info.geometryCount = 1;
    info.pGeometries = &geometry;

    VkBufferDeviceAddressInfo addressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addressInfo.buffer = m_scratchBuffer;
    info.scratchData.deviceAddress = vkGetBufferDeviceAddress((VkDevice)m_pDevice->GetHandle(), &addressInfo);
    
    VkAccelerationStructureInstanceKHR* instanceDescs = (VkAccelerationStructureInstanceKHR*)((const char*)m_instanceBufferCpuAddress + m_currentInstanceBufferOffset);
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        memcpy(&instanceDescs[i].transform, instances[i].transform, sizeof(float) * 12);
        instanceDescs[i].instanceCustomIndex = instances[i].instance_id;
        instanceDescs[i].mask = instances[i].instance_mask;
        instanceDescs[i].instanceShaderBindingTableRecordOffset = 0;
        instanceDescs[i].flags = ToVulkanGeometryInstanceFlags(instances[i].flags);
        instanceDescs[i].accelerationStructureReference = ((VulkanRayTracingBLAS*)instances[i].blas)->GetGpuAddress();
    }

    m_currentInstanceBufferOffset += sizeof(VkAccelerationStructureInstanceKHR) * instance_count;
}
