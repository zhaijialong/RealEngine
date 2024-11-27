#pragma once

#include "vulkan_header.h"
#include "vulkan_deletion_queue.h"
#include "../gfx_device.h"
#include "EASTL/hash_map.h"
#include "xxHash/xxhash.h"

namespace eastl
{
    template <>
    struct hash<GfxTextureDesc>
    {
        size_t operator()(const GfxTextureDesc& desc) const
        {
            return XXH3_64bits(&desc, sizeof(desc));
        }
    };
}

class VulkanDevice : public IGfxDevice
{
public:
    VulkanDevice(const GfxDeviceDesc& desc);
    ~VulkanDevice();

    virtual bool Create() override;
    virtual void* GetHandle() const override { return m_device; }
    virtual void BeginFrame() override;
    virtual void EndFrame() override;

    virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name) override;
    virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name) override;
    virtual IGfxFence* CreateFence(const eastl::string& name) override;
    virtual IGfxHeap* CreateHeap(const GfxHeapDesc& desc, const eastl::string& name) override;
    virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name) override;
    virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const eastl::string& name) override;
    virtual IGfxShader* CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name) override;
    virtual IGfxPipelineState* CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name) override;
    virtual IGfxPipelineState* CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name) override;
    virtual IGfxPipelineState* CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name) override;
    virtual IGfxDescriptor* CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name) override;
    virtual IGfxDescriptor* CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name) override;
    virtual IGfxDescriptor* CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name) override;
    virtual IGfxDescriptor* CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name) override;
    virtual IGfxRayTracingBLAS* CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name) override;
    virtual IGfxRayTracingTLAS* CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name) override;

    virtual uint32_t GetAllocationSize(const GfxTextureDesc& desc) override;
    virtual bool DumpMemoryStats(const eastl::string& file) override;

    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice GetDevice() const { return m_device; }
    VmaAllocator GetVmaAllocator() const { return m_vmaAllocator; }
    VmaPool GetSharedResourcePool() const { return m_vmaSharedResourcePool; }
    uint32_t GetGraphicsQueueIndex() const { return m_graphicsQueueIndex; }
    uint32_t GetComputeQueueIndex() const { return m_computeQueueIndex; }
    uint32_t GetCopyQueueIndex() const { return m_copyQueueIndex; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetComputeQueue() const { return m_computeQueue; }
    VkQueue GetCopyQueue() const { return m_copyQueue; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    class VulkanDescriptorAllocator* GetResourceDescriptorAllocator() const { return m_resourceDescriptorAllocator; }
    class VulkanDescriptorAllocator* GetSamplerDescriptorAllocator() const { return m_samplerDescriptorAllocator; }
    class VulkanConstantBufferAllocator* GetConstantBufferAllocator() const;
    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& GetDescriptorBufferProperties() const { return m_descriptorBufferProperties; }

    uint32_t AllocateResourceDescriptor(void** descriptor);
    uint32_t AllocateSamplerDescriptor(void** descriptor);
    void FreeResourceDescriptor(uint32_t index);
    void FreeSamplerDescriptor(uint32_t index);

    VkDeviceAddress AllocateConstantBuffer(const void* data, size_t data_size);
    VkDeviceSize AllocateConstantBufferDescriptor(const uint32_t* cbv0, const VkDescriptorAddressInfoEXT& cbv1, const VkDescriptorAddressInfoEXT& cbv2);

    template<typename T>
    void Delete(T objectHandle);

    void EnqueueDefaultLayoutTransition(IGfxTexture* texture);
    void CancelDefaultLayoutTransition(IGfxTexture* texture);
    void FlushLayoutTransition(GfxCommandQueue queue_type);

private:
    VkResult CreateInstance();
    VkResult CreateDevice();
    VkResult CreateVmaAllocator();
    VkResult CreatePipelineLayout();
    void FindQueueFamilyIndex();
    bool CheckExtensions(const eastl::vector<VkExtensionProperties>& availableExts, const eastl::vector<const char*> requiredExts);

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE;
    VmaPool m_vmaSharedResourcePool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout[3] = {};
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_descriptorBufferProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
    VkExportMemoryAllocateInfo m_exportMemory = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
#if RE_PLATFORM_WINDOWS
    VkExportMemoryWin32HandleInfoKHR m_exportMemoryWin32 = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
#endif

    uint32_t m_graphicsQueueIndex = uint32_t(-1);
    uint32_t m_computeQueueIndex = uint32_t(-1);
    uint32_t m_copyQueueIndex = uint32_t(-1);
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkQueue m_copyQueue = VK_NULL_HANDLE;

    VulkanDeletionQueue* m_deferredDeletionQueue = nullptr;
    IGfxCommandList* m_transitionCopyCommandList[GFX_MAX_INFLIGHT_FRAMES] = {};
    IGfxCommandList* m_transitionGraphicsCommandList[GFX_MAX_INFLIGHT_FRAMES] = {};

    class VulkanConstantBufferAllocator* m_constantBufferAllocators[GFX_MAX_INFLIGHT_FRAMES] = {};
    class VulkanDescriptorAllocator* m_resourceDescriptorAllocator = nullptr;
    class VulkanDescriptorAllocator* m_samplerDescriptorAllocator = nullptr;

    eastl::vector<eastl::pair<IGfxTexture*, GfxAccessFlags>> m_pendingGraphicsTransitions;
    eastl::vector<eastl::pair<IGfxTexture*, GfxAccessFlags>> m_pendingCopyTransitions;

    eastl::hash_map<GfxTextureDesc, uint32_t> m_textureSizeMap;
};

template<typename T>
inline void VulkanDevice::Delete(T objectHandle)
{
    if (objectHandle != VK_NULL_HANDLE)
    {
        m_deferredDeletionQueue->Delete(objectHandle, m_frameID);
    }
}
