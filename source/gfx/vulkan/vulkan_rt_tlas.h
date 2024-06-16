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

private:
    VkAccelerationStructureKHR m_accelerationStructure = VK_NULL_HANDLE;
};