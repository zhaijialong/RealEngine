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
#include "vulkan_descriptor_allocator.h"
#include "vulkan_constant_buffer_allocator.h"
#include "utils/log.h"
#include "utils/assert.h"
#include "utils/string.h"
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#define TRACY_VK_USE_SYMBOL_TABLE
#include "tracy/public/tracy/TracyVulkan.hpp"

#if RE_PLATFORM_WINDOWS
#include "nvsdk_ngx_vk.h"
#include "xess/xess_vk.h"
#endif

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
    vkDeviceWaitIdle(m_device);

    for (size_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        delete m_transitionCopyCommandList[i];
        delete m_transitionGraphicsCommandList[i];
        delete m_constantBufferAllocators[i];
    }
    delete m_deferredDeletionQueue;

    delete m_resourceDescriptorAllocator;
    delete m_samplerDescriptorAllocator;

    if (m_pTracyGraphicsQueueCtx)
    {
        TracyVkDestroy(m_pTracyGraphicsQueueCtx);
    }

    if (m_pTracyComputeQueueCtx)
    {
        TracyVkDestroy(m_pTracyComputeQueueCtx);
    }

    vmaDestroyPool(m_vmaAllocator, m_vmaSharedResourcePool);
    vmaDestroyAllocator(m_vmaAllocator);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout[0], nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout[1], nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout[2], nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

bool VulkanDevice::Create()
{
#define CHECK_VK_RESULT(x) if((x) != VK_SUCCESS) return false;

    CHECK_VK_RESULT(volkInitialize());
    CHECK_VK_RESULT(CreateInstance());
    CHECK_VK_RESULT(CreateDevice());
    CHECK_VK_RESULT(CreateVmaAllocator());
    CHECK_VK_RESULT(CreatePipelineLayout());
    CHECK_VK_RESULT(CreateTracyCtx());

    m_deferredDeletionQueue = new VulkanDeletionQueue(this);

    for (size_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_transitionCopyCommandList[i] = CreateCommandList(GfxCommandQueue::Copy, "Transition CommandList(Copy)");
        m_transitionGraphicsCommandList[i] = CreateCommandList(GfxCommandQueue::Graphics, "Transition CommandList(Graphics)");

        m_constantBufferAllocators[i] = new VulkanConstantBufferAllocator(this, 8 * 1024 * 1024);
    }

    VkPhysicalDeviceProperties2 properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties.pNext = &m_descriptorBufferProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);

    size_t resourceDescriptorSize = m_descriptorBufferProperties.sampledImageDescriptorSize;
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.storageImageDescriptorSize);
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.robustUniformTexelBufferDescriptorSize);
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.robustStorageTexelBufferDescriptorSize);
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.robustUniformBufferDescriptorSize);
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.robustStorageBufferDescriptorSize);
    resourceDescriptorSize = eastl::max(resourceDescriptorSize, m_descriptorBufferProperties.accelerationStructureDescriptorSize);

    size_t samplerDescriptorSize = m_descriptorBufferProperties.samplerDescriptorSize;

    m_resourceDescriptorAllocator = new VulkanDescriptorAllocator(this, (uint32_t)resourceDescriptorSize, GFX_MAX_RESOURCE_DESCRIPTOR_COUNT, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
    m_samplerDescriptorAllocator = new VulkanDescriptorAllocator(this, (uint32_t)samplerDescriptorSize, GFX_MAX_SAMPLER_DESCRIPTOR_COUNT, VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT);

    return true;
#undef CHECK_VK_RESULT
}

void VulkanDevice::BeginFrame()
{
    m_deferredDeletionQueue->Flush();

    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;
    m_transitionCopyCommandList[index]->ResetAllocator();
    m_transitionGraphicsCommandList[index]->ResetAllocator();

    m_constantBufferAllocators[index]->Reset();
}

void VulkanDevice::EndFrame()
{
    ++m_frameID;

    vmaSetCurrentFrameIndex(m_vmaAllocator, (uint32_t)m_frameID);
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
    VulkanCommandList* commandList = new VulkanCommandList(this, queue_type, name);
    if (!commandList->Create())
    {
        delete commandList;
        return nullptr;
    }
    return commandList;
}

IGfxFence* VulkanDevice::CreateFence(const eastl::string& name)
{
    VulkanFence* fence = new VulkanFence(this, name);
    if (!fence->Create())
    {
        delete fence;
        return nullptr;
    }
    return fence;
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
    VulkanShader* shader = new VulkanShader(this, desc, name);
    if (!shader->Create(data))
    {
        delete shader;
        return nullptr;
    }
    return shader;
}

IGfxPipelineState* VulkanDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    VulkanGraphicsPipelineState* pipeline = new VulkanGraphicsPipelineState(this, desc, name);
    if (!pipeline->Create())
    {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

IGfxPipelineState* VulkanDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    VulkanMeshShadingPipelineState* pipeline = new VulkanMeshShadingPipelineState(this, desc, name);
    if (!pipeline->Create())
    {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

IGfxPipelineState* VulkanDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    VulkanComputePipelineState* pipeline = new VulkanComputePipelineState(this, desc, name);
    if (!pipeline->Create())
    {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
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
    VulkanRayTracingBLAS* blas = new VulkanRayTracingBLAS(this, desc, name);
    if (!blas->Create())
    {
        delete blas;
        return nullptr;
    }
    return blas;
}

IGfxRayTracingTLAS* VulkanDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    VulkanRayTracingTLAS* tlas = new VulkanRayTracingTLAS(this, desc, name);
    if (!tlas->Create())
    {
        delete tlas;
        return nullptr;
    }
    return tlas;
}

uint32_t VulkanDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    auto iter = m_textureSizeMap.find(desc);
    if (iter != m_textureSizeMap.end())
    {
        return iter->second;
    }

    VkImageCreateInfo createInfo = ToVulkanImageCreateInfo(desc);
    VkImage image;
    VkResult result = vkCreateImage(m_device, &createInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        return 0;
    }

    VkImageMemoryRequirementsInfo2 info = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
    info.image = image;

    VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    vkGetImageMemoryRequirements2(m_device, &info, &requirements);

    vkDestroyImage(m_device, image, nullptr);

    m_textureSizeMap.emplace(desc, requirements.memoryRequirements.size);
    return (uint32_t)requirements.memoryRequirements.size;
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
        //"VK_EXT_surface_maintenance1",
        "VK_EXT_debug_utils",
    };

#if RE_PLATFORM_WINDOWS
    unsigned int dlssInstanceExtCount, dlssDeviceExtCount;
    const char** dlssInstanceExts;
    const char** dlssDeviceExts;
    NVSDK_NGX_VULKAN_RequiredExtensions(&dlssInstanceExtCount, &dlssInstanceExts, &dlssDeviceExtCount, &dlssDeviceExts);

    for (unsigned int i = 0; i < dlssInstanceExtCount; ++i)
    {
        auto found = eastl::find(required_extensions.begin(), required_extensions.end(), dlssInstanceExts[i],
            [](const char* a, const char* b)
            {
                return stricmp(a, b) == 0;
            });

        if (found == required_extensions.end())
        {
            required_extensions.push_back(dlssInstanceExts[i]);
        }
    }

    uint32_t xessInstanceExtensionsCount;
    const char* const* xessInstanceExtensions;
    uint32_t xessMinApiVersion;
    xessVKGetRequiredInstanceExtensions(&xessInstanceExtensionsCount, &xessInstanceExtensions, &xessMinApiVersion);

    for (unsigned int i = 0; i < xessInstanceExtensionsCount; ++i)
    {
        auto found = eastl::find(required_extensions.begin(), required_extensions.end(), xessInstanceExtensions[i],
            [](const char* a, const char* b)
            {
                return stricmp(a, b) == 0;
            });

        if (found == required_extensions.end())
        {
            required_extensions.push_back(xessInstanceExtensions[i]);
        }
    }
#endif

    if (!CheckExtensions(extensions, required_extensions))
    {
        return VK_ERROR_UNKNOWN;
    }

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

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

    RE_INFO("GPU : {}", physicalDeviceProperties.deviceName);
    RE_INFO("API version : {}.{}.{}", VK_API_VERSION_MAJOR(physicalDeviceProperties.apiVersion), 
        VK_API_VERSION_MINOR(physicalDeviceProperties.apiVersion), VK_API_VERSION_PATCH(physicalDeviceProperties.apiVersion));

    switch (physicalDeviceProperties.vendorID)
    {
    case 0x1002:
        m_vendor = GfxVendor::AMD;
        break;
    case 0x10DE:
        m_vendor = GfxVendor::Nvidia;
        break;
    case 0x8086:
        m_vendor = GfxVendor::Intel;
        break;
    default:
        break;
    }

    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, NULL, &extension_count, NULL);
    eastl::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, NULL, &extension_count, extensions.data());

    RE_DEBUG("available extensions:");
    for (uint32_t i = 0; i < extension_count; ++i)
    {
        RE_DEBUG("    {}", extensions[i].extensionName);
    }

    eastl::vector<const char*> required_extensions =
    {
        "VK_KHR_swapchain",
        "VK_KHR_swapchain_mutable_format",
        "VK_KHR_maintenance1",
        "VK_KHR_maintenance2",
        "VK_KHR_maintenance3",
        "VK_KHR_maintenance4",
        "VK_KHR_buffer_device_address",
        "VK_KHR_deferred_host_operations",
        "VK_KHR_acceleration_structure",
        "VK_KHR_ray_query",
        "VK_KHR_dynamic_rendering",
        "VK_KHR_synchronization2",
        "VK_KHR_copy_commands2",
        "VK_KHR_bind_memory2",
        "VK_KHR_timeline_semaphore",
        "VK_KHR_dedicated_allocation",
        "VK_EXT_calibrated_timestamps",
#if RE_PLATFORM_WINDOWS
        "VK_KHR_external_memory_win32",
#endif
        "VK_EXT_mesh_shader",
        "VK_EXT_descriptor_indexing",
        "VK_EXT_mutable_descriptor_type",
        "VK_EXT_descriptor_buffer",
        "VK_EXT_scalar_block_layout",
    };

#if RE_PLATFORM_WINDOWS
    if (m_vendor == GfxVendor::Nvidia)
    {
        unsigned int dlssInstanceExtCount, dlssDeviceExtCount;
        const char** dlssInstanceExts;
        const char** dlssDeviceExts;
        NVSDK_NGX_VULKAN_RequiredExtensions(&dlssInstanceExtCount, &dlssInstanceExts, &dlssDeviceExtCount, &dlssDeviceExts);

        for (unsigned int i = 0; i < dlssDeviceExtCount; ++i)
        {
            auto found = eastl::find(required_extensions.begin(), required_extensions.end(), dlssDeviceExts[i],
                [](const char* a, const char* b)
                {
                    return stricmp(a, b) == 0;
                });

            if (found == required_extensions.end())
            {
                required_extensions.push_back(dlssDeviceExts[i]);
            }
        }
    }

    uint32_t xessExtensionsCount;
    const char* const* xessExtensions;
    xessVKGetRequiredDeviceExtensions(m_instance, m_physicalDevice, &xessExtensionsCount, &xessExtensions);

    for (uint32_t i = 0; i < xessExtensionsCount; ++i)
    {
        auto found = eastl::find(required_extensions.begin(), required_extensions.end(), xessExtensions[i],
            [](const char* a, const char* b)
            {
                return stricmp(a, b) == 0;
            });

        if (found == required_extensions.end())
        {
            required_extensions.push_back(xessExtensions[i]);
        }
    }
#endif

    if (!CheckExtensions(extensions, required_extensions))
    {
        return VK_ERROR_UNKNOWN;
    }

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

    VkPhysicalDeviceVulkan12Features vulkan12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    vulkan12.shaderInt8 = VK_TRUE;
    vulkan12.shaderFloat16 = VK_TRUE;
    vulkan12.descriptorIndexing = VK_TRUE;
    vulkan12.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
    vulkan12.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
    vulkan12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12.runtimeDescriptorArray = VK_TRUE;
    vulkan12.samplerFilterMinmax = VK_TRUE;
    vulkan12.scalarBlockLayout = VK_TRUE;
    vulkan12.timelineSemaphore = VK_TRUE;
    vulkan12.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    vulkan13.pNext = &vulkan12;
    vulkan13.inlineUniformBlock = VK_TRUE;
    vulkan13.synchronization2 = VK_TRUE;
    vulkan13.dynamicRendering = VK_TRUE;
    vulkan13.subgroupSizeControl = VK_TRUE;
    vulkan13.shaderDemoteToHelperInvocation = VK_TRUE;
    vulkan13.shaderIntegerDotProduct = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    accelerationStructure.pNext = &vulkan13;
    accelerationStructure.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQuery = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    rayQuery.pNext = &accelerationStructure;
    rayQuery.rayQuery = VK_TRUE;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShader = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    meshShader.pNext = &rayQuery;
    meshShader.meshShader = VK_TRUE;
    meshShader.taskShader = VK_TRUE;

    VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableDescriptor = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT };
    mutableDescriptor.pNext = &meshShader;
    mutableDescriptor.mutableDescriptorType = VK_TRUE;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBuffer = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
    descriptorBuffer.pNext = &mutableDescriptor;
    descriptorBuffer.descriptorBuffer = VK_TRUE;
    //descriptorBuffer.descriptorBufferImageLayoutIgnored = VK_TRUE;

    VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_info.pNext = &descriptorBuffer;
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

    VkResult result = vmaCreateAllocator(&create_info, &m_vmaAllocator);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = 1024; // Doesn't matter
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    uint32_t memoryTypeIndex;
    vmaFindMemoryTypeIndexForBufferInfo(m_vmaAllocator, &bufferInfo, &allocInfo, &memoryTypeIndex);

    VmaPoolCreateInfo poolInfo = {};
    poolInfo.memoryTypeIndex = memoryTypeIndex;
    poolInfo.pMemoryAllocateNext = &m_exportMemory;

#if RE_PLATFORM_WINDOWS
    m_exportMemory.pNext = &m_exportMemoryWin32;
    m_exportMemory.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    m_exportMemoryWin32.dwAccess = GENERIC_ALL;
#endif

    return vmaCreatePool(m_vmaAllocator, &poolInfo, &m_vmaSharedResourcePool);
}

VkResult VulkanDevice::CreatePipelineLayout()
{
    VkDescriptorType mutableDescriptorTypes[7] =
    {
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    VkMutableDescriptorTypeListEXT mutableDescriptorList;
    mutableDescriptorList.descriptorTypeCount = 7;
    mutableDescriptorList.pDescriptorTypes = mutableDescriptorTypes;

    VkMutableDescriptorTypeCreateInfoEXT mutableDescriptorInfo = { VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT };
    mutableDescriptorInfo.mutableDescriptorTypeListCount = 1;
    mutableDescriptorInfo.pMutableDescriptorTypeLists = &mutableDescriptorList;

    VkDescriptorSetLayoutBinding constantBuffer[GFX_MAX_CBV_BINDINGS] = {};
    constantBuffer[0].descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    constantBuffer[0].descriptorCount = sizeof(uint32_t) * GFX_MAX_ROOT_CONSTANTS;
    constantBuffer[0].stageFlags = VK_SHADER_STAGE_ALL;

    for (uint32_t i = 1; i < GFX_MAX_CBV_BINDINGS; ++i)
    {
        constantBuffer[i].binding = i;
        constantBuffer[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        constantBuffer[i].descriptorCount = 1;
        constantBuffer[i].stageFlags = VK_SHADER_STAGE_ALL;
    }

    VkDescriptorSetLayoutBinding resourceDescriptorHeap = {};
    resourceDescriptorHeap.descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
    resourceDescriptorHeap.descriptorCount = GFX_MAX_RESOURCE_DESCRIPTOR_COUNT;
    resourceDescriptorHeap.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutBinding samplerDescriptorHeap = {};
    samplerDescriptorHeap.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerDescriptorHeap.descriptorCount = GFX_MAX_SAMPLER_DESCRIPTOR_COUNT;
    samplerDescriptorHeap.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo0 = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; // constant buffers
    setLayoutInfo0.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    setLayoutInfo0.bindingCount = GFX_MAX_CBV_BINDINGS;
    setLayoutInfo0.pBindings = constantBuffer;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo1 = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; // resource descriptor heap
    setLayoutInfo1.pNext = &mutableDescriptorInfo;
    setLayoutInfo1.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    setLayoutInfo1.bindingCount = 1;
    setLayoutInfo1.pBindings = &resourceDescriptorHeap;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo2 = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; // sampler descriptor heap
    setLayoutInfo2.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    setLayoutInfo2.bindingCount = 1;
    setLayoutInfo2.pBindings = &samplerDescriptorHeap;

    vkCreateDescriptorSetLayout(m_device, &setLayoutInfo0, nullptr, &m_descriptorSetLayout[0]);
    vkCreateDescriptorSetLayout(m_device, &setLayoutInfo1, nullptr, &m_descriptorSetLayout[1]);
    vkCreateDescriptorSetLayout(m_device, &setLayoutInfo2, nullptr, &m_descriptorSetLayout[2]);

    VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.setLayoutCount = 3;
    createInfo.pSetLayouts = m_descriptorSetLayout;

    return vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);
}

VkResult VulkanDevice::CreateTracyCtx()
{
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandBufferAllocateInfo commandBufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount = 1;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    {
        poolInfo.queueFamilyIndex = m_graphicsQueueIndex;
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool);

        commandBufferInfo.commandPool = commandPool;
        vkAllocateCommandBuffers(m_device, &commandBufferInfo, &commandBuffer);

        m_pTracyGraphicsQueueCtx = TracyVkContextCalibrated(m_instance, m_physicalDevice, m_device, m_graphicsQueue, commandBuffer, vkGetInstanceProcAddr, vkGetDeviceProcAddr);

        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, nullptr);
    }

    {
        poolInfo.queueFamilyIndex = m_computeQueueIndex;
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool);

        commandBufferInfo.commandPool = commandPool;
        vkAllocateCommandBuffers(m_device, &commandBufferInfo, &commandBuffer);

        m_pTracyComputeQueueCtx = TracyVkContextCalibrated(m_instance, m_physicalDevice, m_device, m_computeQueue, commandBuffer, vkGetInstanceProcAddr, vkGetDeviceProcAddr);

        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, nullptr);
    }

    return VK_SUCCESS;
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

bool VulkanDevice::CheckExtensions(const eastl::vector<VkExtensionProperties>& availableExts, const eastl::vector<const char*> requiredExts)
{
    for (size_t i = 0; i < requiredExts.size(); i++)
    {
        auto found = eastl::find(availableExts.begin(), availableExts.end(), requiredExts[i],
            [](const VkExtensionProperties& extensionProperties, const char* ext)
            {
                return stricmp(extensionProperties.extensionName, ext) == 0;
            });

        if (found == availableExts.end())
        {
            RE_DEBUG("unsupported extension : {}", requiredExts[i]);
            return false;
        }
    }

    return true;
}

void VulkanDevice::EnqueueDefaultLayoutTransition(IGfxTexture* texture)
{
    // vulkan images is always in undefined layout after created, we translate them to default layouts which match d3d12 behaviors

    const GfxTextureDesc& desc = texture->GetDesc();

    if (((VulkanTexture*)texture)->IsSwapchainTexture())
    {
        m_pendingGraphicsTransitions.emplace_back(texture, GfxAccessPresent);
    }
    else if (desc.usage & GfxTextureUsageRenderTarget)
    {
        m_pendingGraphicsTransitions.emplace_back(texture, GfxAccessRTV);
    }
    else if (desc.usage & GfxTextureUsageDepthStencil)
    {
        m_pendingGraphicsTransitions.emplace_back(texture, GfxAccessDSV);
    }
    else if (desc.usage & GfxTextureUsageUnorderedAccess)
    {
        m_pendingGraphicsTransitions.emplace_back(texture, GfxAccessMaskUAV);
    }
    else
    {
        m_pendingCopyTransitions.emplace_back(texture, GfxAccessCopyDst);
    }
}

void VulkanDevice::CancelDefaultLayoutTransition(IGfxTexture* texture)
{
    auto iter = eastl::find(m_pendingGraphicsTransitions.begin(), m_pendingGraphicsTransitions.end(), texture, 
        [](const eastl::pair<IGfxTexture*, GfxAccessFlags>& transition, IGfxTexture* texture)
        {
            return transition.first == texture;
        });

    if (iter != m_pendingGraphicsTransitions.end())
    {
        m_pendingGraphicsTransitions.erase(iter);
    }

    iter = eastl::find(m_pendingCopyTransitions.begin(), m_pendingCopyTransitions.end(), texture,
        [](const eastl::pair<IGfxTexture*, GfxAccessFlags>& transition, IGfxTexture* texture)
        {
            return transition.first == texture;
        });

    if (iter != m_pendingCopyTransitions.end())
    {
        m_pendingCopyTransitions.erase(iter);
    }
}

void VulkanDevice::FlushLayoutTransition(GfxCommandQueue queue_type)
{
    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;

    if (queue_type == GfxCommandQueue::Graphics) // todo : compute queue ?
    {
        if (!m_pendingGraphicsTransitions.empty() || !m_pendingCopyTransitions.empty())
        {
            m_transitionGraphicsCommandList[index]->Begin();
            
            for (size_t i = 0; i < m_pendingGraphicsTransitions.size(); ++i)
            {
                m_transitionGraphicsCommandList[index]->TextureBarrier(m_pendingGraphicsTransitions[i].first, GFX_ALL_SUB_RESOURCE, GfxAccessDiscard, m_pendingGraphicsTransitions[i].second);
            }
            m_pendingGraphicsTransitions.clear();

            for (size_t i = 0; i < m_pendingCopyTransitions.size(); ++i)
            {
                m_transitionGraphicsCommandList[index]->TextureBarrier(m_pendingCopyTransitions[i].first, GFX_ALL_SUB_RESOURCE, GfxAccessDiscard, m_pendingCopyTransitions[i].second);
            }
            m_pendingCopyTransitions.clear();

            m_transitionGraphicsCommandList[index]->End();
            m_transitionGraphicsCommandList[index]->Submit();
        }
    }
    
    if (queue_type == GfxCommandQueue::Copy)
    {
        if (!m_pendingCopyTransitions.empty())
        {
            m_transitionCopyCommandList[index]->Begin();

            for (size_t i = 0; i < m_pendingCopyTransitions.size(); ++i)
            {
                m_transitionCopyCommandList[index]->TextureBarrier(m_pendingCopyTransitions[i].first, GFX_ALL_SUB_RESOURCE, GfxAccessDiscard, m_pendingCopyTransitions[i].second);
            }
            m_pendingCopyTransitions.clear();

            m_transitionCopyCommandList[index]->End();
            m_transitionCopyCommandList[index]->Submit();
        }
    }
}

uint32_t VulkanDevice::AllocateResourceDescriptor(void** descriptor)
{
    return m_resourceDescriptorAllocator->Allocate(descriptor);
}

uint32_t VulkanDevice::AllocateSamplerDescriptor(void** descriptor)
{
    return m_samplerDescriptorAllocator->Allocate(descriptor);
}

void VulkanDevice::FreeResourceDescriptor(uint32_t index)
{
    if (index != GFX_INVALID_RESOURCE)
    {
        m_deferredDeletionQueue->FreeResourceDescriptor(index, m_frameID);
    }
}

void VulkanDevice::FreeSamplerDescriptor(uint32_t index)
{
    if (index != GFX_INVALID_RESOURCE)
    {
        m_deferredDeletionQueue->FreeSamplerDescriptor(index, m_frameID);
    }
}

VulkanConstantBufferAllocator* VulkanDevice::GetConstantBufferAllocator() const
{
    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;
    return m_constantBufferAllocators[index];
}

VkDeviceAddress VulkanDevice::AllocateConstantBuffer(const void* data, size_t data_size)
{
    void* cpuAddress;
    VkDeviceAddress gpuAddress;
    GetConstantBufferAllocator()->Allocate((uint32_t)data_size, &cpuAddress, &gpuAddress);

    memcpy(cpuAddress, data, data_size);

    return gpuAddress;
}

VkDeviceSize VulkanDevice::AllocateConstantBufferDescriptor(const uint32_t* cbv0, const VkDescriptorAddressInfoEXT& cbv1, const VkDescriptorAddressInfoEXT& cbv2)
{
    size_t descriptorBufferSize = sizeof(uint32_t) * GFX_MAX_ROOT_CONSTANTS + m_descriptorBufferProperties.robustUniformBufferDescriptorSize * 2;
    void* cpuAddress;
    VkDeviceAddress gpuAddress;
    GetConstantBufferAllocator()->Allocate((uint32_t)descriptorBufferSize, &cpuAddress, &gpuAddress);

    memcpy(cpuAddress, cbv0, sizeof(uint32_t) * GFX_MAX_ROOT_CONSTANTS);

    VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    if (cbv1.address != 0)
    {
        descriptorInfo.data.pUniformBuffer = &cbv1;
        vkGetDescriptorEXT(m_device, &descriptorInfo, m_descriptorBufferProperties.robustUniformBufferDescriptorSize, 
            (char*)cpuAddress + sizeof(uint32_t) * GFX_MAX_ROOT_CONSTANTS);
    }

    if (cbv2.address != 0)
    {
        descriptorInfo.data.pUniformBuffer = &cbv2;
        vkGetDescriptorEXT(m_device, &descriptorInfo, m_descriptorBufferProperties.robustUniformBufferDescriptorSize, 
            (char*)cpuAddress + sizeof(uint32_t) * GFX_MAX_ROOT_CONSTANTS + m_descriptorBufferProperties.robustUniformBufferDescriptorSize);
    }

    VkDeviceSize descriptorBufferOffset = gpuAddress - GetConstantBufferAllocator()->GetGpuAddress();
    return descriptorBufferOffset;
}
