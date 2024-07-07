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
    MetalConstantBufferAllocator(MTL::Device* device, uint32_t buffer_size, const eastl::string& name)
    {
        m_pBuffer = device->newBuffer(buffer_size, MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeUntracked);
        m_pCpuAddress = m_pBuffer->contents();
        m_bufferSize = buffer_size;
    }
    
    ~MetalConstantBufferAllocator()
    {
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
    MTL::Buffer* m_pBuffer = nullptr;
    void* m_pCpuAddress = nullptr;
    uint32_t m_allocatedSize = 0;
    uint32_t m_bufferSize = 0;
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
    
    m_pQueue = m_pDevice->newCommandQueue();
    
    for (uint32_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("CB Allocator {}", i).c_str();
        m_pConstantBufferAllocators[i] = eastl::make_unique<MetalConstantBufferAllocator>(m_pDevice, 8 * 1024 * 1024, name);
    }

    return true;
}

void MetalDevice::BeginFrame()
{
    uint32_t index = m_frameID % GFX_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocators[index]->Reset();
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
