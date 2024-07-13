#pragma once

#include "metal_utils.h"
#include "../gfx_device.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/queue.h"

class MetalDevice : public IGfxDevice
{
public:
    MetalDevice(const GfxDeviceDesc& desc);
    ~MetalDevice();
    
    virtual bool Create() override;
    virtual void* GetHandle() const override { return m_pDevice; }
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
    
    MTL::CommandQueue* GetQueue() const { return m_pQueue; }
    
    uint64_t AllocateConstantBuffer(const void* data, size_t data_size);
    
    uint32_t AllocateResourceDescriptor(IRDescriptorTableEntry** descriptor);
    uint32_t AllocateSamplerDescriptor(IRDescriptorTableEntry** descriptor);
    void FreeResourceDescriptor(uint32_t index);
    void FreeSamplerDescriptor(uint32_t index);
    MTL::Buffer* GetResourceDescriptorBuffer() const;
    MTL::Buffer* GetSamplerDescriptorBuffer() const;
    
    void MakeResident(const MTL::Allocation* allocation);
    void Evict(const MTL::Allocation* allocation);
    
private:
    MTL::Device* m_pDevice = nullptr;
    MTL::CommandQueue* m_pQueue = nullptr;
    MTL::ResidencySet* m_pResidencySet = nullptr;
    bool m_bResidencyDirty = false;
    
    eastl::unique_ptr<class MetalConstantBufferAllocator> m_pConstantBufferAllocators[GFX_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<class MetalDescriptorAllocator> m_pResDescriptorAllocator;
    eastl::unique_ptr<class MetalDescriptorAllocator> m_pSamplerAllocator;
    
    eastl::queue<eastl::pair<uint32_t, uint64_t>> m_resDescriptorDeletionQueue;
    eastl::queue<eastl::pair<uint32_t, uint64_t>> m_samplerDescriptorDeletionQueue;
};
