#pragma once

#include "core/platform.h"
#include "../gfx_defines.h"
#include "utils/assert.h"

#define VK_NO_PROTOTYPES

#if RE_PLATFORM_WINDOWS
    #define VK_USE_PLATFORM_WIN32_KHR
#elif RE_PLATFORM_MAC
    #define VK_USE_PLATFORM_MACOS_MVK
#elif RE_PLATFORM_IOS
    #define VK_USE_PLATFORM_IOS_MVK
#endif

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"


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

inline VkImageCreateInfo ToVulkanImageCreateInfo(const GfxTextureDesc& desc)
{
    VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    createInfo.imageType = desc.type == GfxTextureType::Texture3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    createInfo.format = ToVulkanFormat(desc.format);
    createInfo.extent.width = desc.width;
    createInfo.extent.height = desc.height;
    createInfo.extent.depth = desc.depth;
    createInfo.mipLevels = desc.mip_levels;
    createInfo.arrayLayers = desc.array_size;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (desc.usage & GfxTextureUsageRenderTarget)
    {
        createInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (desc.usage & GfxTextureUsageDepthStencil)
    {
        createInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (desc.usage & GfxTextureUsageUnorderedAccess)
    {
        createInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (desc.type == GfxTextureType::TextureCube || desc.type == GfxTextureType::TextureCubeArray)
    {
        RE_ASSERT(desc.array_size % 6 == 0);
        createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    if (desc.alloc_type == GfxAllocationType::Sparse)
    {
        createInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    }

    createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    return createInfo;
}

inline VkImageAspectFlags GetAspectFlags(GfxFormat format)
{
    VkImageAspectFlags aspectMask = 0;

    if (format == GfxFormat::D32FS8)
    {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if (format == GfxFormat::D32F || format == GfxFormat::D16)
    {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else
    {
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    return aspectMask;
}

inline VkPipelineStageFlags2 GetStageMask(GfxAccessFlags flags)
{
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;

    if (flags & GfxAccessPresent)         stage |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    if (flags & GfxAccessRTV)             stage |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (flags & GfxAccessMaskDSV)         stage |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    if (flags & GfxAccessMaskVS)          stage |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if (flags & GfxAccessMaskPS)          stage |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if (flags & GfxAccessMaskCS)          stage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (flags & GfxAccessMaskCopy)        stage |= VK_PIPELINE_STAGE_2_COPY_BIT;
    if (flags & GfxAccessClearUAV)        stage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (flags & GfxAccessShadingRate)     stage |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    if (flags & GfxAccessIndexBuffer)     stage |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    if (flags & GfxAccessIndirectArgs)    stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    if (flags & GfxAccessMaskAS)          stage |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

    return stage;
}

inline VkAccessFlags2 GetAccessMask(GfxAccessFlags flags)
{
    VkAccessFlags2 access = VK_ACCESS_2_NONE;

    if (flags & GfxAccessDiscard)
    {
        return access;
    }

    if (flags & GfxAccessRTV)             access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    if (flags & GfxAccessDSV)             access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (flags & GfxAccessDSVReadOnly)     access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (flags & GfxAccessMaskSRV)         access |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    if (flags & GfxAccessMaskUAV)         access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    if (flags & GfxAccessClearUAV)        access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    if (flags & GfxAccessCopyDst)         access |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    if (flags & GfxAccessCopySrc)         access |= VK_ACCESS_2_TRANSFER_READ_BIT;
    if (flags & GfxAccessShadingRate)     access |= VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
    if (flags & GfxAccessIndexBuffer)     access |= VK_ACCESS_2_INDEX_READ_BIT;
    if (flags & GfxAccessIndirectArgs)    access |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    if (flags & GfxAccessASRead)          access |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    if (flags & GfxAccessASWrite)         access |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

    return access;
}

inline VkImageLayout GetImageLayout(GfxAccessFlags flags)
{
    if (flags & GfxAccessDiscard)         return VK_IMAGE_LAYOUT_UNDEFINED;
    if (flags & GfxAccessPresent)         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if (flags & GfxAccessRTV)             return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (flags & GfxAccessDSV)             return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (flags & GfxAccessDSVReadOnly)     return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (flags & GfxAccessMaskSRV)         return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (flags & GfxAccessMaskUAV)         return VK_IMAGE_LAYOUT_GENERAL;
    if (flags & GfxAccessClearUAV)        return VK_IMAGE_LAYOUT_GENERAL;
    if (flags & GfxAccessCopyDst)         return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (flags & GfxAccessCopySrc)         return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (flags & GfxAccessShadingRate)     return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

    RE_ASSERT(false);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline VkAttachmentLoadOp GetLoadOp(GfxRenderPassLoadOp load_op)
{
    switch (load_op)
    {
    case GfxRenderPassLoadOp::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case GfxRenderPassLoadOp::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case GfxRenderPassLoadOp::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    default:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
}

inline VkAttachmentStoreOp GetStoreOp(GfxRenderPassStoreOp store_op)
{
    switch (store_op)
    {
    case GfxRenderPassStoreOp::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case GfxRenderPassStoreOp::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    default:
        return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

inline VkPrimitiveTopology ToVkPrimitiveTopology(GfxPrimitiveType type)
{
    switch (type)
    {
    case GfxPrimitiveType::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case GfxPrimitiveType::LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GfxPrimitiveType::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case GfxPrimitiveType::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GfxPrimitiveType::TriangleTrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
}

inline VkCullModeFlags GetCullMode(GfxCullMode mode)
{
    switch (mode)
    {
    case GfxCullMode::None:
        return VK_CULL_MODE_NONE;
    case GfxCullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case GfxCullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    default:
        return VK_CULL_MODE_NONE;
    }
}

inline VkPipelineRasterizationStateCreateInfo ToVkPipelineRasterizationStateCreateInfo(const GfxRasterizerState& state)
{
    VkPipelineRasterizationStateCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    createInfo.depthClampEnable = !state.depth_clip;
    createInfo.polygonMode = state.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    createInfo.cullMode = GetCullMode(state.cull_mode);
    createInfo.frontFace = state.front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    createInfo.depthBiasEnable = state.depth_bias != 0.0 || state.depth_slope_scale != 0.0;
    createInfo.depthBiasConstantFactor = state.depth_bias;
    createInfo.depthBiasClamp = state.depth_bias_clamp;
    createInfo.depthBiasSlopeFactor = state.depth_slope_scale;

    return createInfo;
}

inline VkCompareOp ToVkCompareOp(GfxCompareFunc func)
{
    switch (func)
    {
    case GfxCompareFunc::Never:
        return VK_COMPARE_OP_NEVER;
    case GfxCompareFunc::Less:
        return VK_COMPARE_OP_LESS;
    case GfxCompareFunc::Equal:
        return VK_COMPARE_OP_EQUAL;
    case GfxCompareFunc::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case GfxCompareFunc::Greater:
        return VK_COMPARE_OP_GREATER;
    case GfxCompareFunc::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case GfxCompareFunc::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case GfxCompareFunc::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return VK_COMPARE_OP_MAX_ENUM;
    }
}

inline VkStencilOp ToVkStencilOp(GfxStencilOp stencilOp)
{
    switch (stencilOp)
    {
    case GfxStencilOp::Keep:
        return VK_STENCIL_OP_KEEP;
    case GfxStencilOp::Zero:
        return VK_STENCIL_OP_ZERO;
    case GfxStencilOp::Replace:
        return VK_STENCIL_OP_REPLACE;
    case GfxStencilOp::IncreaseClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case GfxStencilOp::DecreaseClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case GfxStencilOp::Invert:
        return VK_STENCIL_OP_INVERT;
    case GfxStencilOp::IncreaseWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case GfxStencilOp::DecreaseWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
        return VK_STENCIL_OP_MAX_ENUM;
    }
}

inline VkStencilOpState ToVkStencilOpState(GfxDepthStencilOp state, uint8_t readMask, uint8_t writeMask)
{
    VkStencilOpState stencilOpState = {};
    stencilOpState.failOp = ToVkStencilOp(state.stencil_fail);
    stencilOpState.depthFailOp = ToVkStencilOp(state.depth_fail);
    stencilOpState.passOp = ToVkStencilOp(state.pass);
    stencilOpState.compareOp = ToVkCompareOp(state.stencil_func);
    stencilOpState.compareMask = readMask;
    stencilOpState.writeMask = writeMask;

    return stencilOpState;
}

inline VkPipelineDepthStencilStateCreateInfo ToVkPipelineDepthStencilStateCreateInfo(const GfxDepthStencilState& state)
{
    VkPipelineDepthStencilStateCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    createInfo.depthTestEnable = state.depth_test;
    createInfo.depthWriteEnable = state.depth_write;
    createInfo.depthCompareOp = ToVkCompareOp(state.depth_func);
    createInfo.stencilTestEnable = state.stencil_test;
    createInfo.front = ToVkStencilOpState(state.front, state.stencil_read_mask, state.stencil_write_mask);
    createInfo.back = ToVkStencilOpState(state.back, state.stencil_read_mask, state.stencil_write_mask);

    return createInfo;
}

inline VkBlendFactor ToVkBlendFactor(GfxBlendFactor blendFactor, bool alpha = false)
{
    switch (blendFactor)
    {
    case GfxBlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case GfxBlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case GfxBlendFactor::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case GfxBlendFactor::InvSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GfxBlendFactor::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case GfxBlendFactor::InvSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GfxBlendFactor::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case GfxBlendFactor::InvDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GfxBlendFactor::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case GfxBlendFactor::InvDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GfxBlendFactor::SrcAlphaClamp:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case GfxBlendFactor::ConstantFactor:
        return alpha ? VK_BLEND_FACTOR_CONSTANT_ALPHA : VK_BLEND_FACTOR_CONSTANT_COLOR;
    case GfxBlendFactor::InvConstantFactor:
        return alpha ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    default:
        return VK_BLEND_FACTOR_MAX_ENUM;
    }
}

inline VkBlendOp ToVkBlendOp(GfxBlendOp blendOp)
{
    switch (blendOp)
    {
    case GfxBlendOp::Add:
        return VK_BLEND_OP_ADD;
    case GfxBlendOp::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case GfxBlendOp::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case GfxBlendOp::Min:
        return VK_BLEND_OP_MIN;
    case GfxBlendOp::Max:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_MAX_ENUM;
    }
}

inline VkPipelineColorBlendStateCreateInfo ToVkPipelineColorBlendStateCreateInfo(const GfxBlendState* states, VkPipelineColorBlendAttachmentState* vkStates)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        vkStates[i].blendEnable = states[i].blend_enable;
        vkStates[i].srcColorBlendFactor = ToVkBlendFactor(states[i].color_src);
        vkStates[i].dstColorBlendFactor = ToVkBlendFactor(states[i].color_dst);
        vkStates[i].colorBlendOp = ToVkBlendOp(states[i].color_op);
        vkStates[i].srcAlphaBlendFactor = ToVkBlendFactor(states[i].alpha_src, true);
        vkStates[i].dstAlphaBlendFactor = ToVkBlendFactor(states[i].alpha_dst, true);
        vkStates[i].alphaBlendOp = ToVkBlendOp(states[i].alpha_op);
        vkStates[i].colorWriteMask = states[i].write_mask;
    }

    VkPipelineColorBlendStateCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    createInfo.attachmentCount = 8;
    createInfo.pAttachments = vkStates;

    return createInfo;
}

template<typename T>
inline VkPipelineRenderingCreateInfo ToVkPipelineRenderingCreateInfo(const T& pipelineDesc, VkFormat* colorFormats)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        colorFormats[i] = ToVulkanFormat(pipelineDesc.rt_format[i], true);
    }

    VkPipelineRenderingCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    createInfo.colorAttachmentCount = 8;
    createInfo.pColorAttachmentFormats = colorFormats;
    createInfo.depthAttachmentFormat = ToVulkanFormat(pipelineDesc.depthstencil_format);

    if (pipelineDesc.depthstencil_format == GfxFormat::D32FS8)
    {
        createInfo.stencilAttachmentFormat = createInfo.depthAttachmentFormat;
    }

    return createInfo;
}

inline VkFilter ToVkFilter(GfxFilter filter)
{
    switch (filter)
    {
    case GfxFilter::Point:
        return VK_FILTER_NEAREST;
    case GfxFilter::Linear:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_MAX_ENUM;
    }
}

inline VkSamplerMipmapMode ToVkSamplerMipmapMode(GfxFilter filter)
{
    switch (filter)
    {
    case GfxFilter::Point:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case GfxFilter::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

inline VkSamplerAddressMode ToVkSamplerAddressMode(GfxSamplerAddressMode mode)
{
    switch (mode)
    {
    case GfxSamplerAddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case GfxSamplerAddressMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case GfxSamplerAddressMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case GfxSamplerAddressMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

inline VkSamplerReductionMode ToVkSamplerReductionMode(GfxSamplerReductionMode mode)
{
    switch (mode)
    {
    case GfxSamplerReductionMode::Standard:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    case GfxSamplerReductionMode::Compare:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    case GfxSamplerReductionMode::Min:
        return VK_SAMPLER_REDUCTION_MODE_MIN;
    case GfxSamplerReductionMode::Max:
        return VK_SAMPLER_REDUCTION_MODE_MAX;
    default:
        return VK_SAMPLER_REDUCTION_MODE_MAX_ENUM;
    }
}