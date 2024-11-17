#pragma once

#include "vulkan_header.h"
#include "../gfx_rt_blas.h"

class VulkanDevice;

class VulkanRayTracingBLAS : public IGfxRayTracingBLAS
{
public:
    VulkanRayTracingBLAS(VulkanDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name);
    ~VulkanRayTracingBLAS();

    virtual void* GetHandle() const override { return m_accelerationStructure; }

    bool Create();
    VkDeviceAddress GetGpuAddress() const;
    void GetBuildInfo(VkAccelerationStructureBuildGeometryInfoKHR& info);
    void GetUpdateInfo(VkAccelerationStructureBuildGeometryInfoKHR& info, VkAccelerationStructureGeometryKHR& geometry, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset);
    const VkAccelerationStructureBuildRangeInfoKHR* GetBuildRangeInfo() const { return m_rangeInfos.data(); }

private:
    VkAccelerationStructureKHR m_accelerationStructure = VK_NULL_HANDLE;
    VkBuffer m_asBuffer = VK_NULL_HANDLE;
    VmaAllocation m_asBufferAllocation = VK_NULL_HANDLE;
    VkBuffer m_scratchBuffer = VK_NULL_HANDLE;
    VmaAllocation m_scratchBufferAllocation = VK_NULL_HANDLE;

    eastl::vector<VkAccelerationStructureGeometryKHR> m_geometries;
    eastl::vector<VkAccelerationStructureBuildRangeInfoKHR> m_rangeInfos;
};