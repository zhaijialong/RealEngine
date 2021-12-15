#pragma once

#include "d3d12_header.h"
#include "../i_gfx_device.h"

#include <queue>

namespace D3D12MA
{
    class Allocator;
    class Allocation;
}

class D3D12DescriptorAllocator
{
public:
    D3D12DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count, const std::string& name);
    ~D3D12DescriptorAllocator();

    D3D12Descriptor Allocate();
    void Free(const D3D12Descriptor& descriptor);

    ID3D12DescriptorHeap* GetHeap() const { return m_pHeap; }
    D3D12Descriptor GetDescriptor(uint32_t index) const;

private:
    ID3D12DescriptorHeap* m_pHeap = nullptr;
    uint32_t m_descriptorSize = 0;
    uint32_t m_descirptorCount = 0;
    uint32_t m_allocatedCount = 0;
    bool m_bShaderVisible = false;
    std::vector<D3D12Descriptor> m_freeDescriptors;
};

class D3D12Device;

class D3D12ConstantBufferAllocator
{
public:
    D3D12ConstantBufferAllocator(D3D12Device* device, uint32_t buffer_size, const std::string& name);

    void Allocate(uint32_t size, void** cpu_address, uint64_t* gpu_address);
    void Reset();
private:
    std::unique_ptr<IGfxBuffer> m_pBuffer = nullptr;
    uint32_t m_allcatedSize = 0;
};

class D3D12Device : public IGfxDevice
{
public:
    D3D12Device(const GfxDeviceDesc& desc);
    ~D3D12Device();

    virtual void BeginFrame() override;
    virtual void EndFrame() override;
    virtual uint64_t GetFrameID() const override { return m_nFrameID; }
    virtual void* GetHandle() const override { return m_pDevice; }
    virtual GfxVendor GetVendor() const override { return m_vendor; }

    virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, const std::string& name) override;
    virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const std::string& name) override;
    virtual IGfxFence* CreateFence(const std::string& name) override;
    virtual IGfxHeap* CreateHeap(const GfxHeapDesc& desc, const std::string& name) override;
    virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const std::string& name) override;
    virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, IGfxHeap* heap, uint32_t offset, const std::string& name) override;
    virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const std::string& name) override;
    virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, IGfxHeap* heap, uint32_t offset, const std::string& name) override;
    virtual IGfxShader* CreateShader(const GfxShaderDesc& desc, const std::vector<uint8_t>& data, const std::string& name) override;
    virtual IGfxPipelineState* CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name) override;
    virtual IGfxPipelineState* CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const std::string& name) override;
    virtual IGfxPipelineState* CreateComputePipelineState(const GfxComputePipelineDesc& desc, const std::string& name) override;
    virtual IGfxDescriptor* CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const std::string& name) override;
    virtual IGfxDescriptor* CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const std::string& name) override;
    virtual IGfxDescriptor* CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const std::string& name) override;
    virtual IGfxDescriptor* CreateSampler(const GfxSamplerDesc& desc, const std::string& name) override;

    virtual uint32_t GetAllocationSize(const GfxTextureDesc& desc) override;
    virtual bool DumpMemoryStats(const std::string& file) override;

    bool Init();
    IDXGIFactory5* GetDxgiFactory() const { return m_pDxgiFactory; }
    ID3D12CommandQueue* GetGraphicsQueue() const { return m_pGraphicsQueue; }
    ID3D12CommandQueue* GetComputeQueue() const { return m_pComputeQueue; }
    ID3D12CommandQueue* GetCopyQueue() const { return m_pCopyQueue; }
    D3D12MA::Allocator* GetResourceAllocator() const { return m_pResourceAllocator; }
    ID3D12DescriptorHeap* GetResourceDescriptorHeap() const { return m_pResDescriptorAllocator->GetHeap(); }
    ID3D12DescriptorHeap* GetSamplerDescriptorHeap() const { return m_pSamplerAllocator->GetHeap(); }
    ID3D12RootSignature* GetRootSignature() const { return m_pRootSignature; }
    ID3D12CommandSignature* GetDrawSignature() const { return m_pDrawSignature; }
    ID3D12CommandSignature* GetDrawIndexedSignature() const { return m_pDrawIndexedSignature; }
    ID3D12CommandSignature* GetDispatchSignature() const { return m_pDispatchSignature; }
    ID3D12CommandSignature* GetDispatchMeshSignature() const { return m_pDispatchMeshSignature; }

    D3D12_GPU_VIRTUAL_ADDRESS AllocateConstantBuffer(const void* data, size_t data_size);

    void FlushDeferredDeletions();
    void Delete(IUnknown* object);
    void Delete(D3D12MA::Allocation* allocation);

    D3D12Descriptor AllocateRTV();
    D3D12Descriptor AllocateDSV();
    D3D12Descriptor AllocateResourceDescriptor();
    D3D12Descriptor AllocateSampler();
    void DeleteRTV(const D3D12Descriptor& descriptor);
    void DeleteDSV(const D3D12Descriptor& descriptor);
    void DeleteResourceDescriptor(const D3D12Descriptor& descriptor);
    void DeleteSampler(const D3D12Descriptor& descriptor);


#if MICROPROFILE_GPU_TIMERS_D3D12
    int GetProfileGraphicsQueue() const { return m_nProfileGraphicsQueue; }
    int GetProfileComputeQueue() const { return m_nProfileComputeQueue; }
    int GetProfileCopyQueue() const { return m_nProfileCopyQueue; }
#endif

private:
    void DoDeferredDeletion(bool force_delete = false);
    void CreateRootSignature();

    void CreateIndirectCommandSignatures();

private:
    GfxDeviceDesc m_desc;
    GfxVendor m_vendor;

    IDXGIFactory5* m_pDxgiFactory = nullptr;
    IDXGIAdapter1* m_pDxgiAdapter = nullptr;

    ID3D12Device2* m_pDevice = nullptr;
    ID3D12CommandQueue* m_pGraphicsQueue = nullptr;
    ID3D12CommandQueue* m_pComputeQueue = nullptr;
    ID3D12CommandQueue* m_pCopyQueue = nullptr;
    ID3D12RootSignature* m_pRootSignature = nullptr;

    ID3D12CommandSignature* m_pDrawSignature = nullptr;
    ID3D12CommandSignature* m_pDrawIndexedSignature = nullptr;
    ID3D12CommandSignature* m_pDispatchSignature = nullptr;
    ID3D12CommandSignature* m_pDispatchMeshSignature = nullptr;

    D3D12MA::Allocator* m_pResourceAllocator = nullptr;

    static const uint32_t CB_ALLOCATOR_COUNT = 3;
    std::unique_ptr<D3D12ConstantBufferAllocator> m_pConstantBufferAllocators[CB_ALLOCATOR_COUNT];

    std::unique_ptr<D3D12DescriptorAllocator> m_pRTVAllocator;
    std::unique_ptr<D3D12DescriptorAllocator> m_pDSVAllocator;
    std::unique_ptr<D3D12DescriptorAllocator> m_pResDescriptorAllocator;
    std::unique_ptr<D3D12DescriptorAllocator> m_pSamplerAllocator;

    uint64_t m_nFrameID = 0;

    struct ObjectDeletion
    {
        IUnknown* object;
        uint64_t frame;
    };
    std::queue<ObjectDeletion> m_deletionQueue;

    struct AllocationDeletion
    {
        D3D12MA::Allocation* allocation;
        uint64_t frame;
    };
    std::queue<AllocationDeletion> m_allocationDeletionQueue;

    struct DescriptorDeletion
    {
        D3D12Descriptor descriptor;
        uint64_t frame;
    };
    std::queue<DescriptorDeletion> m_rtvDeletionQueue;
    std::queue<DescriptorDeletion> m_dsvDeletionQueue;
    std::queue<DescriptorDeletion> m_resourceDeletionQueue;
    std::queue<DescriptorDeletion> m_samplerDeletionQueue;

#if MICROPROFILE_GPU_TIMERS_D3D12
    int m_nProfileGraphicsQueue = -1;
    int m_nProfileComputeQueue = -1;
    int m_nProfileCopyQueue = -1;
#endif
};