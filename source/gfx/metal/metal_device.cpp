#include "metal_device.h"
#include "metal_buffer.h"
#include "metal_texture.h"
#include "metal_fence.h"
#include "metal_swapchain.h"
#include "metal_command_list.h"
#include "metal_shader.h"
#include "metal_pipeline_state.h"
#include "metal_descriptor.h"
#include "metal_heap.h"
#include "metal_rt_blas.h"
#include "metal_rt_tlas.h"
#include "../gfx.h"
#include "utils/log.h"
#include "utils/math.h"

class MetalConstantBufferAllocator
{
public:
    MetalConstantBufferAllocator(MetalDevice* device, uint32_t buffer_size, const eastl::string& name)
    {
        m_pDevice = device;
        
        MTL::Device* mtlDevice = (MTL::Device*)m_pDevice->GetHandle();
        m_pBuffer = mtlDevice->newBuffer(buffer_size, MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeTracked);
        m_pDevice->MakeResident(m_pBuffer);
        SetDebugLabel(m_pBuffer, name.c_str());
        
        m_pCpuAddress = m_pBuffer->contents();
        m_bufferSize = buffer_size;
    }
    
    ~MetalConstantBufferAllocator()
    {
        m_pDevice->Evict(m_pBuffer);
        m_pBuffer->release();
    }

    void Allocate(uint32_t size, void** cpu_address, uint64_t* gpu_address)
    {
        RE_ASSERT(m_allocatedSize + size <= m_bufferSize);

        *cpu_address = (char*)m_pCpuAddress + m_allocatedSize;
        *gpu_address = m_pBuffer->gpuAddress() + m_allocatedSize;

        m_allocatedSize += RoundUpPow2(size, 8); // Shader converter requires an alignment of 8-bytes:
    }
    
    void Reset()
    {
        m_allocatedSize = 0;
    }
    
private:
    MetalDevice* m_pDevice = nullptr;
    MTL::Buffer* m_pBuffer = nullptr;
    void* m_pCpuAddress = nullptr;
    uint32_t m_bufferSize = 0;
    uint32_t m_allocatedSize = 0;
};

class MetalDescriptorAllocator
{
public:
    MetalDescriptorAllocator(MetalDevice* device, uint32_t descriptor_count, const eastl::string& name)
    {
        m_pDevice = device;
        
        MTL::Device* mtlDevice = (MTL::Device*)m_pDevice->GetHandle();
        m_pBuffer = mtlDevice->newBuffer(sizeof(IRDescriptorTableEntry) * descriptor_count,
            MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeTracked);
        m_pDevice->MakeResident(m_pBuffer);
        SetDebugLabel(m_pBuffer, name.c_str());
        
        m_pCpuAddress = m_pBuffer->contents();
        m_descirptorCount = descriptor_count;
    }
    
    ~MetalDescriptorAllocator()
    {
        m_pDevice->Evict(m_pBuffer);
        m_pBuffer->release();
    }
    
    uint32_t Allocate(IRDescriptorTableEntry** descriptor)
    {
        uint32_t index = 0;
        
        if(!m_freeDescriptors.empty())
        {
            index = m_freeDescriptors.back();
            m_freeDescriptors.pop_back();
        }
        else
        {
            index = m_allocatedCount;
            ++m_allocatedCount;
        }
        
        *descriptor = (IRDescriptorTableEntry*)((char*)m_pCpuAddress + sizeof(IRDescriptorTableEntry) * index);
        return index;
    }
    
    void Free(uint32_t index)
    {
        m_freeDescriptors.push_back(index);
    }
    
    MTL::Buffer* GetBuffer() const { return m_pBuffer; }
    
private:
    MetalDevice* m_pDevice = nullptr;
    MTL::Buffer* m_pBuffer = nullptr;
    void* m_pCpuAddress = nullptr;
    uint32_t m_descirptorCount = 0;
    uint32_t m_allocatedCount = 0;
    eastl::vector<uint32_t> m_freeDescriptors;
};

MetalDevice::MetalDevice(const GfxDeviceDesc& desc)
{
    m_desc = desc;
    m_vendor = GfxVendor::Apple;
}

MetalDevice::~MetalDevice()
{
    for (uint32_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBufferAllocators[i].reset();
    }
    
    m_pResDescriptorAllocator.reset();
    m_pSamplerAllocator.reset();
    
    m_pResidencySet->release();
    m_pQueue->release();
    m_pDevice->release();
}

bool MetalDevice::Create()
{
    m_pDevice = MTL::CreateSystemDefaultDevice();
    
    RE_INFO("GPU : {}", m_pDevice->name()->utf8String());
    
    if(!m_pDevice->supportsFamily(MTL::GPUFamilyApple7))
    {
        RE_INFO("RealEngine requires a Metal GPU family Apple7+ device");
        return false;
    }
    
    MTL::ResidencySetDescriptor* descriptor = MTL::ResidencySetDescriptor::alloc()->init();
    descriptor->setInitialCapacity(GFX_MAX_RESOURCE_DESCRIPTOR_COUNT);
    m_pResidencySet = m_pDevice->newResidencySet(descriptor, nullptr);
    descriptor->release();
    
    m_pQueue = m_pDevice->newCommandQueue();
    m_pQueue->addResidencySet(m_pResidencySet);
    
    for (uint32_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("CB Allocator {}", i).c_str();
        m_pConstantBufferAllocators[i] = eastl::make_unique<MetalConstantBufferAllocator>(this, 8 * 1024 * 1024, name);
    }

    m_pResDescriptorAllocator = eastl::make_unique<MetalDescriptorAllocator>(this, GFX_MAX_RESOURCE_DESCRIPTOR_COUNT, "Resource Heap");
    m_pSamplerAllocator = eastl::make_unique<MetalDescriptorAllocator>(this, GFX_MAX_SAMPLER_DESCRIPTOR_COUNT, "Sampler Heap");

    return true;
}

void MetalDevice::BeginFrame()
{
    if(m_bResidencyDirty)
    {
        m_pResidencySet->commit();
        m_bResidencyDirty = false;
    }
    
    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocators[index]->Reset();
    
    while (!m_resDescriptorDeletionQueue.empty())
    {
        auto item = m_resDescriptorDeletionQueue.front();
        if (item.second + GFX_MAX_INFLIGHT_FRAMES > m_frameID)
        {
            break;
        }

        m_pResDescriptorAllocator->Free(item.first);
        m_resDescriptorDeletionQueue.pop();
    }

    while (!m_samplerDescriptorDeletionQueue.empty())
    {
        auto item = m_samplerDescriptorDeletionQueue.front();
        if (item.second + GFX_MAX_INFLIGHT_FRAMES > m_frameID)
        {
            break;
        }

        m_pSamplerAllocator->Free(item.first);
        m_samplerDescriptorDeletionQueue.pop();
    }
}

void MetalDevice::EndFrame()
{
    ++m_frameID;
}

IGfxSwapchain* MetalDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    MetalSwapchain* swapchain = new MetalSwapchain(this, desc, name);
    if (!swapchain->Create())
    {
        delete swapchain;
        return nullptr;
    }
    return swapchain;
}

IGfxCommandList* MetalDevice::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    MetalCommandList* commandList = new MetalCommandList(this, queue_type, name);
    if (!commandList->Create())
    {
        delete commandList;
        return nullptr;
    }
    return commandList;
}

IGfxFence* MetalDevice::CreateFence(const eastl::string& name)
{
    MetalFence* fence = new MetalFence(this, name);
    if (!fence->Create())
    {
        delete fence;
        return nullptr;
    }
    return fence;
}

IGfxHeap* MetalDevice::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    MetalHeap* heap = new MetalHeap(this, desc, name);
    if (!heap->Create())
    {
        delete heap;
        return nullptr;
    }
    return heap;
}

IGfxBuffer* MetalDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    MetalBuffer* buffer = new MetalBuffer(this, desc, name);
    if (!buffer->Create())
    {
        delete buffer;
        return nullptr;
    }
    return buffer;
}

IGfxTexture* MetalDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    MetalTexture* texture = new MetalTexture(this, desc, name);
    if (!texture->Create())
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

IGfxShader* MetalDevice::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    MetalShader* shader = new MetalShader(this, desc, name);
    if (!shader->Create(data))
    {
        delete shader;
        return nullptr;
    }
    return shader;
}

IGfxPipelineState* MetalDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    MetalGraphicsPipelineState* pso = new MetalGraphicsPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* MetalDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    MetalMeshShadingPipelineState* pso = new MetalMeshShadingPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* MetalDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    MetalComputePipelineState* pso = new MetalComputePipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxDescriptor* MetalDevice::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    MetalShaderResourceView* srv = new MetalShaderResourceView(this, resource, desc, name);
    if (!srv->Create())
    {
        delete srv;
        return nullptr;
    }
    return srv;
}

IGfxDescriptor* MetalDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    MetalUnorderedAccessView* uav = new MetalUnorderedAccessView(this, resource, desc, name);
    if (!uav->Create())
    {
        delete uav;
        return nullptr;
    }
    return uav;
}

IGfxDescriptor* MetalDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    MetalConstantBufferView* cbv = new MetalConstantBufferView(this, buffer, desc, name);
    if (!cbv->Create())
    {
        delete cbv;
        return nullptr;
    }
    return cbv;
}

IGfxDescriptor* MetalDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    MetalSampler* sampler = new MetalSampler(this, desc, name);
    if (!sampler->Create())
    {
        delete sampler;
        return nullptr;
    }
    return sampler;
}

IGfxRayTracingBLAS* MetalDevice::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    MetalRayTracingBLAS* blas = new MetalRayTracingBLAS(this, desc, name);
    if (!blas->Create())
    {
        delete blas;
        return nullptr;
    }
    return blas;
}

IGfxRayTracingTLAS* MetalDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    MetalRayTracingTLAS* tlas = new MetalRayTracingTLAS(this, desc, name);
    if (!tlas->Create())
    {
        delete tlas;
        return nullptr;
    }
    return tlas;
}

uint32_t MetalDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    MTL::TextureDescriptor* descriptor = ToTextureDescriptor(desc);
    MTL::SizeAndAlign sizeAndAlign = m_pDevice->heapTextureSizeAndAlign(descriptor);
    descriptor->release();
    
    return (uint32_t)sizeAndAlign.size;
}

bool MetalDevice::DumpMemoryStats(const eastl::string& file)
{
    return false;
}

uint64_t MetalDevice::AllocateConstantBuffer(const void* data, size_t data_size)
{
    void* cpu_address;
    uint64_t gpu_address;

    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocators[index]->Allocate((uint32_t)data_size, &cpu_address, &gpu_address);

    memcpy(cpu_address, data, data_size);

    return gpu_address;
}

uint32_t MetalDevice::AllocateResourceDescriptor(IRDescriptorTableEntry** descriptor)
{
    return m_pResDescriptorAllocator->Allocate(descriptor);
}

uint32_t MetalDevice::AllocateSamplerDescriptor(IRDescriptorTableEntry** descriptor)
{
    return m_pSamplerAllocator->Allocate(descriptor);
}

void MetalDevice::FreeResourceDescriptor(uint32_t index)
{
    if(index != GFX_INVALID_RESOURCE)
    {
        m_resDescriptorDeletionQueue.push(eastl::make_pair(index, m_frameID));
    }
}

void MetalDevice::FreeSamplerDescriptor(uint32_t index)
{
    if(index != GFX_INVALID_RESOURCE)
    {
        m_samplerDescriptorDeletionQueue.push(eastl::make_pair(index, m_frameID));
    }
}

MTL::Buffer* MetalDevice::GetResourceDescriptorBuffer() const
{
    return m_pResDescriptorAllocator->GetBuffer();
}

MTL::Buffer* MetalDevice::GetSamplerDescriptorBuffer() const
{
    return m_pSamplerAllocator->GetBuffer();
}

void MetalDevice::MakeResident(const MTL::Allocation* allocation)
{
    m_pResidencySet->addAllocation(allocation);
    m_bResidencyDirty = true;
}

void MetalDevice::Evict(const MTL::Allocation* allocation)
{
    m_pResidencySet->removeAllocation(allocation);
    m_bResidencyDirty = true;
}
