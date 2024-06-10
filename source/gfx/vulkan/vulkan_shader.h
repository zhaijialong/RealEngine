#pragma once

#include "vulkan_header.h"
#include "../gfx_shader.h"

class VulkanDevice;

class VulkanShader : public IGfxShader
{
public:
    VulkanShader(VulkanDevice* pDevice, const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name);
    ~VulkanShader();

    virtual void* GetHandle() const override { return m_shader; }
    virtual bool SetShaderData(const uint8_t* data, uint32_t data_size) override;

private:
    VkShaderModule m_shader = VK_NULL_HANDLE;
};