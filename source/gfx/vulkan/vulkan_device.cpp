#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"
#include "vulkan_fence.h"
#include "vulkan_swapchain.h"
#include "vulkan_command_list.h"
#include "vulkan_shader.h"
#include "vulkan_pipeline_state.h"
#include "vulkan_descriptor.h"
#include "vulkan_heap.h"
#include "vulkan_rt_blas.h"
#include "vulkan_rt_tlas.h"
#include "utils/log.h"
#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

static VkBool32 VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    RE_DEBUG(pCallbackData->pMessage);

    return VK_FALSE;
}

VulkanDevice::VulkanDevice(const GfxDeviceDesc& desc)
{
    m_desc = desc;
}

VulkanDevice::~VulkanDevice()
{
    //vkDestroyDevice(m_device, nullptr);
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

bool VulkanDevice::Init()
{
#define CHECK_VK_RESULT(x) if((x) != VK_SUCCESS) return false;

    CHECK_VK_RESULT(volkInitialize());
    CHECK_VK_RESULT(CreateInstance());
    volkLoadInstance(m_instance);
    CHECK_VK_RESULT(CreateDebugMessenger());

    return true;
}

void VulkanDevice::BeginFrame()
{
}

void VulkanDevice::EndFrame()
{
    ++m_frameID;
}

uint64_t VulkanDevice::GetFrameID() const
{
    return 0;
}

void* VulkanDevice::GetHandle() const
{
    return nullptr;
}

GfxVendor VulkanDevice::GetVendor() const
{
    return GfxVendor();
}

IGfxSwapchain* VulkanDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxCommandList* VulkanDevice::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    return nullptr;
}

IGfxFence* VulkanDevice::CreateFence(const eastl::string& name)
{
    return nullptr;
}

IGfxHeap* VulkanDevice::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxBuffer* VulkanDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxTexture* VulkanDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxShader* VulkanDevice::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* VulkanDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* VulkanDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* VulkanDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* VulkanDevice::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* VulkanDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* VulkanDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* VulkanDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxRayTracingBLAS* VulkanDevice::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxRayTracingTLAS* VulkanDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    return nullptr;
}

uint32_t VulkanDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    return 0;
}

bool VulkanDevice::DumpMemoryStats(const eastl::string& file)
{
    return false;
}

VkResult VulkanDevice::CreateInstance()
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    eastl::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    eastl::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

    RE_DEBUG("available instance layers:");
    for (uint32_t i = 0; i < layer_count; ++i)
    {
        RE_DEBUG("    {}", layers[i].layerName);
    }

    RE_DEBUG("available instance extensions:");
    for (uint32_t i = 0; i < extension_count; ++i)
    {
        RE_DEBUG("    {}", extensions[i].extensionName);
    }

    eastl::vector<const char*> required_layers = 
    {
#if defined(_DEBUG) || defined(DEBUG)
        "VK_LAYER_KHRONOS_validation",
#endif
    };

    eastl::vector<const char*> required_extensions = 
    {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        "VK_EXT_debug_utils",
    };

    //todo : check if the required layers/extensions are available

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pEngineName = "RealEngine";
    appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = (uint32_t)required_layers.size();
    createInfo.ppEnabledLayerNames = required_layers.data();
    createInfo.enabledExtensionCount = (uint32_t)required_extensions.size();
    createInfo.ppEnabledExtensionNames = required_extensions.data();

    return vkCreateInstance(&createInfo, nullptr, &m_instance);
}

VkResult VulkanDevice::CreateDebugMessenger()
{
#if defined(_DEBUG) || defined(DEBUG)
    VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = VulkanDebugCallback;

    return vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
#else
    return VK_SUCCESS;
#endif
}
