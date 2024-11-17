#pragma once

#include "vulkan_header.h"
#include "../gfx_rt_tlas.h"

class VulkanDevice;

class VulkanRayTracingTLAS : public IGfxRayTracingTLAS
{
public:
    VulkanRayTracingTLAS(VulkanDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name);
    ~VulkanRayTracingTLAS();

    virtual void* GetHandle() const override { return m_accelerationStructure; }

    bool Create();
    VkDeviceAddress GetGpuAddress() const;
    void GetBuildInfo(VkAccelerationStructureBuildGeometryInfoKHR& info, VkAccelerationStructureGeometryKHR& geometry,
        const GfxRayTracingInstance* instances, uint32_t instance_count);

private:
    VkAccelerationStructureKHR m_accelerationStructure = VK_NULL_HANDLE;
    VkBuffer m_asBuffer = VK_NULL_HANDLE;
    VmaAllocation m_asBufferAllocation = VK_NULL_HANDLE;
    VkBuffer m_scratchBuffer = VK_NULL_HANDLE;
    VmaAllocation m_scratchBufferAllocation = VK_NULL_HANDLE;

    VkBuffer m_instanceBuffer = VK_NULL_HANDLE;
    VmaAllocation m_instanceBufferAllocation = VK_NULL_HANDLE;
    VkDeviceAddress m_instanceBufferGpuAddress = 0;
    void* m_instanceBufferCpuAddress = nullptr;
    uint32_t m_instanceBufferSize = 0;
    uint32_t m_currentInstanceBufferOffset = 0;
};