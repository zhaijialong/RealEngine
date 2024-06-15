#pragma once

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"

#include "../gfx_defines.h"

template<typename T>
inline void SetDebugName(VkDevice device, VkObjectType type, T object, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    info.objectType = type;
    info.objectHandle = (uint64_t)object;
    info.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(device, &info);
}

inline VkFormat ToVulkanFormat(GfxFormat format, bool srv_or_rtv = false)
{
    switch (format)
    {
    case GfxFormat::Unknown:
        return VK_FORMAT_UNDEFINED;
    case GfxFormat::RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GfxFormat::RGBA32UI:
        return VK_FORMAT_R32G32B32A32_UINT;
    case GfxFormat::RGBA32SI:
        return VK_FORMAT_R32G32B32A32_SINT;
    case GfxFormat::RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case GfxFormat::RGBA16UI:
        return VK_FORMAT_R16G16B16A16_UINT;
    case GfxFormat::RGBA16SI:
        return VK_FORMAT_R16G16B16A16_SINT;
    case GfxFormat::RGBA16UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case GfxFormat::RGBA16SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case GfxFormat::RGBA8UI:
        return VK_FORMAT_R8G8B8A8_UINT;
    case GfxFormat::RGBA8SI:
        return VK_FORMAT_R8G8B8A8_SINT;
    case GfxFormat::RGBA8UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case GfxFormat::RGBA8SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case GfxFormat::RGBA8SRGB:
        return srv_or_rtv ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    case GfxFormat::BGRA8UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case GfxFormat::BGRA8SRGB:
        return srv_or_rtv ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    case GfxFormat::RGB10A2UI:
        return VK_FORMAT_A2R10G10B10_UINT_PACK32;
    case GfxFormat::RGB10A2UNORM:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case GfxFormat::RGB32F:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case GfxFormat::RGB32UI:
        return VK_FORMAT_R32G32B32_UINT;
    case GfxFormat::RGB32SI:
        return VK_FORMAT_R32G32B32_SINT;
    case GfxFormat::R11G11B10F:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case GfxFormat::RGB9E5:
        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    case GfxFormat::RG32F:
        return VK_FORMAT_R32G32_SFLOAT;
    case GfxFormat::RG32UI:
        return VK_FORMAT_R32G32_UINT;
    case GfxFormat::RG32SI:
        return VK_FORMAT_R32G32_SINT;
    case GfxFormat::RG16F:
        return VK_FORMAT_R16G16_SFLOAT;
    case GfxFormat::RG16UI:
        return VK_FORMAT_R16G16_UINT;
    case GfxFormat::RG16SI:
        return VK_FORMAT_R16G16_SINT;
    case GfxFormat::RG16UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case GfxFormat::RG16SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case GfxFormat::RG8UI:
        return VK_FORMAT_R8G8_UINT;
    case GfxFormat::RG8SI:
        return VK_FORMAT_R8G8_SINT;
    case GfxFormat::RG8UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case GfxFormat::RG8SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case GfxFormat::R32F:
        return VK_FORMAT_R32_SFLOAT;
    case GfxFormat::R32UI:
        return VK_FORMAT_R32_UINT;
    case GfxFormat::R32SI:
        return VK_FORMAT_R32_SINT;
    case GfxFormat::R16F:
        return VK_FORMAT_R16_SFLOAT;
    case GfxFormat::R16UI:
        return VK_FORMAT_R16_UINT;
    case GfxFormat::R16SI:
        return VK_FORMAT_R16_SINT;
    case GfxFormat::R16UNORM:
        return VK_FORMAT_R16_UNORM;
    case GfxFormat::R16SNORM:
        return VK_FORMAT_R16_SNORM;
    case GfxFormat::R8UI:
        return VK_FORMAT_R8_UINT;
    case GfxFormat::R8SI:
        return VK_FORMAT_R8_SINT;
    case GfxFormat::R8UNORM:
        return VK_FORMAT_R8_UNORM;
    case GfxFormat::R8SNORM:
        return VK_FORMAT_R8_SNORM;
    case GfxFormat::D32F:
        return VK_FORMAT_D32_SFLOAT;
    case GfxFormat::D32FS8:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case GfxFormat::D16:
        return VK_FORMAT_D16_UNORM;
    case GfxFormat::BC1UNORM:
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case GfxFormat::BC1SRGB:
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case GfxFormat::BC2UNORM:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case GfxFormat::BC2SRGB:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case GfxFormat::BC3UNORM:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case GfxFormat::BC3SRGB:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case GfxFormat::BC4UNORM:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case GfxFormat::BC4SNORM:
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case GfxFormat::BC5UNORM:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case GfxFormat::BC5SNORM:
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case GfxFormat::BC6U16F:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case GfxFormat::BC6S16F:
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case GfxFormat::BC7UNORM:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case GfxFormat::BC7SRGB:
        return VK_FORMAT_BC7_SRGB_BLOCK;
	default:
        return VK_FORMAT_UNDEFINED;
    }
}

inline VmaMemoryUsage ToVmaUsage(GfxMemoryType type)
{
    switch (type)
    {
    case GfxMemoryType::GpuOnly:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GfxMemoryType::CpuOnly:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GfxMemoryType::CpuToGpu:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case GfxMemoryType::GpuToCpu:
        return VMA_MEMORY_USAGE_GPU_TO_CPU;
    default:
        return VMA_MEMORY_USAGE_AUTO;
    }
}