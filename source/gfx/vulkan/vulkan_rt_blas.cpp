#include "vulkan_rt_blas.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "utils/log.h"

VulkanRayTracingBLAS::VulkanRayTracingBLAS(VulkanDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

VulkanRayTracingBLAS::~VulkanRayTracingBLAS()
{
    VulkanDevice* device = (VulkanDevice*)m_pDevice;
    device->Delete(m_accelerationStructure);
    device->Delete(m_asBuffer);
    device->Delete(m_asBufferAllocation);
    device->Delete(m_scratchBuffer);
    device->Delete(m_scratchBufferAllocation);
}

bool VulkanRayTracingBLAS::Create()
{
    m_geometries.reserve(m_desc.geometries.size());
    m_rangeInfos.reserve(m_desc.geometries.size());
    
    eastl::vector<uint32_t> primitiveCounts;
    primitiveCounts.reserve(m_desc.geometries.size());

    for (size_t i = 0; i < m_desc.geometries.size(); ++i)
    {
        const GfxRayTracingGeometry& geometry = m_desc.geometries[i];

        VkAccelerationStructureGeometryKHR vkGeometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        vkGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        vkGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        vkGeometry.geometry.triangles.vertexFormat = ToVulkanFormat(geometry.vertex_format);
        vkGeometry.geometry.triangles.vertexData.deviceAddress = geometry.vertex_buffer->GetGpuAddress() + geometry.vertex_buffer_offset;
        vkGeometry.geometry.triangles.vertexStride = geometry.vertex_stride;
        vkGeometry.geometry.triangles.maxVertex = geometry.vertex_count - 1;
        vkGeometry.geometry.triangles.indexType = geometry.index_format == GfxFormat::R16UI ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        vkGeometry.geometry.triangles.indexData.deviceAddress = geometry.index_buffer->GetGpuAddress() + geometry.index_buffer_offset;

        if (geometry.opaque)
        {
            vkGeometry.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
        }

        m_geometries.push_back(vkGeometry);
        m_rangeInfos.push_back({ geometry.index_count / 3 });
        primitiveCounts.push_back(geometry.index_count / 3);
    }

    VkDevice device = (VkDevice)m_pDevice->GetHandle();

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    GetBuildInfo(buildInfo);

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primitiveCounts.data(), &sizeInfo);

    //todo : better memory allocation and compaction like what RTXMU does
    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = sizeInfo.accelerationStructureSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VmaAllocator allocator = ((VulkanDevice*)m_pDevice)->GetVmaAllocator();
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &m_asBuffer, &m_asBufferAllocation, nullptr);

    bufferInfo.size = eastl::max(sizeInfo.buildScratchSize, sizeInfo.updateScratchSize);
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &m_scratchBuffer, &m_scratchBufferAllocation, nullptr);

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = m_asBuffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkResult result = vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_accelerationStructure);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanRayTracingBLAS] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, m_accelerationStructure, m_name.c_str());
    SetDebugName(device, VK_OBJECT_TYPE_BUFFER, m_asBuffer, m_name.c_str());
    vmaSetAllocationName(allocator, m_asBufferAllocation, m_name.c_str());

    return true;
}

VkDeviceAddress VulkanRayTracingBLAS::GetGpuAddress() const
{
    VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = m_accelerationStructure;

    return vkGetAccelerationStructureDeviceAddressKHR((VkDevice)m_pDevice->GetHandle(), &info);
}

void VulkanRayTracingBLAS::GetBuildInfo(VkAccelerationStructureBuildGeometryInfoKHR& info)
{
    info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    info.flags = ToVulkanAccelerationStructureFlags(m_desc.flags);
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.dstAccelerationStructure = m_accelerationStructure;
    info.geometryCount = (uint32_t)m_geometries.size();
    info.pGeometries = m_geometries.data();

    if (m_scratchBuffer)
    {
        VkBufferDeviceAddressInfo addressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addressInfo.buffer = m_scratchBuffer;

        info.scratchData.deviceAddress = vkGetBufferDeviceAddress((VkDevice)m_pDevice->GetHandle(), &addressInfo);
    }
}

void VulkanRayTracingBLAS::GetUpdateInfo(VkAccelerationStructureBuildGeometryInfoKHR& info, VkAccelerationStructureGeometryKHR& geometry, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    RE_ASSERT(m_desc.flags & GfxRayTracingASFlagAllowUpdate);
    RE_ASSERT(m_geometries.size() == 1); //todo : suppport more than 1

    geometry = m_geometries[0];
    geometry.geometry.triangles.vertexData.deviceAddress = vertex_buffer->GetGpuAddress() + vertex_buffer_offset;

    GetBuildInfo(info);
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    info.srcAccelerationStructure = m_accelerationStructure;
    info.pGeometries = &geometry;
}

