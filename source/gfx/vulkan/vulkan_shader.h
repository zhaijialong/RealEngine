#pragma once

#include "vulkan_header.h"
#include "../gfx_shader.h"

class VulkanDevice;

class VulkanShader : public IGfxShader
{
public:
    VulkanShader(VulkanDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name);
    ~VulkanShader();

    virtual void* GetHandle() const override { return m_shader; }
    virtual bool Create(eastl::span<uint8_t> data) override;

private:
    VkShaderModule m_shader = VK_NULL_HANDLE;
};