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

private:
    VkAccelerationStructureKHR m_accelerationStructure = VK_NULL_HANDLE;
};