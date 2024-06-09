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
#include "utils/assert.h"
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
    delete m_deferredDeletionQueue;

    vmaDestroyAllocator(m_vmaAllocator);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

bool VulkanDevice::Init()
{
#define CHECK_VK_RESULT(x) if((x) != VK_SUCCESS) return false;

    CHECK_VK_RESULT(volkInitialize());
    CHECK_VK_RESULT(CreateInstance());
    CHECK_VK_RESULT(CreateDevice());
    CHECK_VK_RESULT(CreateVmaAllocator());

    m_deferredDeletionQueue = new VulkanDeletionQueue(this);

    return true;
#undef CHECK_VK_RESULT
}

void VulkanDevice::BeginFrame()
{
    m_deferredDeletionQueue->Flush();
}

void VulkanDevice::EndFrame()
{
    ++m_frameID;
}

IGfxSwapchain* VulkanDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{    
    VulkanSwapchain* swapchain = new VulkanSwapchain(this, desc, name);
    if (!swapchain->Create())
    {
        delete swapchain;
        return nullptr;
    }
    return swapchain;
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
    VulkanHeap* heap = new VulkanHeap(this, desc, name);
    if (!heap->Create())
    {
        delete heap;
        return nullptr;
    }
    return heap;
}

IGfxBuffer* VulkanDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    VulkanBuffer* buffer = new VulkanBuffer(this, desc, name);
    if (!buffer->Create())
    {
        delete buffer;
        return nullptr;
    }
    return buffer;
}

IGfxTexture* VulkanDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    VulkanTexture* texture = new VulkanTexture(this, desc, name);
    if (!texture->Create())
    {
        delete texture;
        return nullptr;
    }
    return texture;
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
    VulkanShaderResourceView* srv = new VulkanShaderResourceView(this, resource, desc, name);
    if (!srv->Create())
    {
        delete srv;
        return nullptr;
    }
    return srv;
}

IGfxDescriptor* VulkanDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    VulkanUnorderedAccessView* uav = new VulkanUnorderedAccessView(this, resource, desc, name);
    if (!uav->Create())
    {
        delete uav;
        return nullptr;
    }
    return uav;
}

IGfxDescriptor* VulkanDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    VulkanConstantBufferView* cbv = new VulkanConstantBufferView(this, buffer, desc, name);
    if (!cbv->Create())
    {
        delete cbv;
        return nullptr;
    }
    return cbv;
}

IGfxDescriptor* VulkanDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    VulkanSampler* sampler = new VulkanSampler(this, desc, name);
    if (!sampler->Create())
    {
        delete sampler;
        return nullptr;
    }
    return sampler;
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

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    volkLoadInstance(m_instance);

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debugMessengerCInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debugMessengerCInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerCInfo.pfnUserCallback = VulkanDebugCallback;

    return vkCreateDebugUtilsMessengerEXT(m_instance, &debugMessengerCInfo, nullptr, &m_debugMessenger);
}

VkResult VulkanDevice::CreateDevice()
{
    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(m_instance, &gpu_count, NULL);
    eastl::vector<VkPhysicalDevice> physical_devices(gpu_count);
    vkEnumeratePhysicalDevices(m_instance, &gpu_count, physical_devices.data());

    m_physicalDevice = physical_devices[0]; //todo : better gpu selection

    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, NULL, &extension_count, NULL);
    eastl::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, NULL, &extension_count, extensions.data());

    RE_DEBUG("available extensions:");
    for (uint32_t i = 0; i < extension_count; ++i)
    {
        RE_DEBUG("    {}", extensions[i].extensionName);
    }

    //todo : check if the required extensions are available
    eastl::vector<const char*> required_extensions =
    {
        "VK_KHR_swapchain",
        "VK_KHR_maintenance4",
        "VK_KHR_buffer_device_address",
        "VK_KHR_deferred_host_operations",
        "VK_KHR_acceleration_structure",
        "VK_KHR_ray_query",
        "VK_EXT_mesh_shader",
        "VK_EXT_descriptor_indexing",
        "VK_EXT_mutable_descriptor_type",
        "VK_KHR_dynamic_rendering",
        "VK_KHR_synchronization2",
        "VK_KHR_copy_commands2",
        "VK_KHR_bind_memory2",
        "VK_KHR_timeline_semaphore",
        "VK_KHR_dedicated_allocation",
    };

    float queue_priorities[1] = { 0.0 };
    FindQueueFamilyIndex();

    VkDeviceQueueCreateInfo queue_info[3];
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].pNext = NULL;
    queue_info[0].flags = 0;
    queue_info[0].queueCount = 1;
    queue_info[0].queueFamilyIndex = m_copyQueueIndex;
    queue_info[0].pQueuePriorities = queue_priorities;
    queue_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[1].pNext = NULL;
    queue_info[1].flags = 0;
    queue_info[1].queueCount = 1;
    queue_info[1].queueFamilyIndex = m_graphicsQueueIndex;
    queue_info[1].pQueuePriorities = queue_priorities;
    queue_info[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[2].pNext = NULL;
    queue_info[2].flags = 0;
    queue_info[2].queueCount = 1;
    queue_info[2].queueFamilyIndex = m_computeQueueIndex;
    queue_info[2].pQueuePriorities = queue_priorities;

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &features);

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

    VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_info.pNext = &bufferDeviceAddress;
    device_info.queueCreateInfoCount = 3;
    device_info.pQueueCreateInfos = queue_info;
    device_info.enabledExtensionCount = (uint32_t)required_extensions.size();
    device_info.ppEnabledExtensionNames = required_extensions.data();
    device_info.pEnabledFeatures = &features; //enable all features supported

    VkResult result = vkCreateDevice(m_physicalDevice, &device_info, NULL, &m_device);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_copyQueueIndex, 0, &m_copyQueue);
    vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_computeQueueIndex, 0, &m_computeQueue);

    return result;
}

VkResult VulkanDevice::CreateVmaAllocator()
{
    VmaVulkanFunctions functions = {};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo create_info = {};
    create_info.flags = VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    create_info.physicalDevice = m_physicalDevice;
    create_info.device = m_device;
    create_info.instance = m_instance;
    create_info.preferredLargeHeapBlockSize = 0;
    create_info.vulkanApiVersion = VK_API_VERSION_1_3;
    create_info.pVulkanFunctions = &functions;

    return vmaCreateAllocator(&create_info, &m_vmaAllocator);
}

void VulkanDevice::FindQueueFamilyIndex()
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queue_family_count, NULL);

    eastl::vector<VkQueueFamilyProperties> queue_family_props(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queue_family_count, queue_family_props.data());

    for (uint32_t i = 0; i < queue_family_count; i++)
    {
        if (m_graphicsQueueIndex == uint32_t(-1))
        {
            if (queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_graphicsQueueIndex = i;
                continue;
            }
        }

        if (m_copyQueueIndex == uint32_t(-1))
        {
            if ((queue_family_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queue_family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                m_copyQueueIndex = i;
                continue;
            }
        }

        if (m_computeQueueIndex == uint32_t(-1))
        {
            if (queue_family_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
                !(queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                m_computeQueueIndex = i;
                continue;
            }
        }
    }

    RE_ASSERT(m_graphicsQueueIndex != uint32_t(-1));
    RE_ASSERT(m_copyQueueIndex != uint32_t(-1));
    RE_ASSERT(m_computeQueueIndex != uint32_t(-1));
}
