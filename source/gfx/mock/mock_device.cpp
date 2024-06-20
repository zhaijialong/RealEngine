#include "mock_device.h"
#include "mock_buffer.h"
#include "mock_texture.h"
#include "mock_fence.h"
#include "mock_swapchain.h"
#include "mock_command_list.h"
#include "mock_shader.h"
#include "mock_pipeline_state.h"
#include "mock_descriptor.h"
#include "mock_heap.h"
#include "mock_rt_blas.h"
#include "mock_rt_tlas.h"
#include "../gfx.h"

MockDevice::MockDevice(const GfxDeviceDesc& desc)
{
    m_desc = desc;
}

MockDevice::~MockDevice()
{
}

bool MockDevice::Create()
{
    return true;
}

void* MockDevice::GetHandle() const
{
    return nullptr;
}

void MockDevice::BeginFrame()
{
}

void MockDevice::EndFrame()
{
    ++m_frameID;
}

IGfxSwapchain* MockDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    MockSwapchain* swapchain = new MockSwapchain(this, desc, name);
    if (!swapchain->Create())
    {
        delete swapchain;
        return nullptr;
    }
    return swapchain;
}

IGfxCommandList* MockDevice::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    MockCommandList* commandList = new MockCommandList(this, queue_type, name);
    if (!commandList->Create())
    {
        delete commandList;
        return nullptr;
    }
    return commandList;
}

IGfxFence* MockDevice::CreateFence(const eastl::string& name)
{
    MockFence* fence = new MockFence(this, name);
    if (!fence->Create())
    {
        delete fence;
        return nullptr;
    }
    return fence;
}

IGfxHeap* MockDevice::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    MockHeap* heap = new MockHeap(this, desc, name);
    if (!heap->Create())
    {
        delete heap;
        return nullptr;
    }
    return heap;
}

IGfxBuffer* MockDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    MockBuffer* buffer = new MockBuffer(this, desc, name);
    if (!buffer->Create())
    {
        delete buffer;
        return nullptr;
    }
    return buffer;
}

IGfxTexture* MockDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    MockTexture* texture = new MockTexture(this, desc, name);
    if (!texture->Create())
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

IGfxShader* MockDevice::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    MockShader* shader = new MockShader(this, desc, name);
    if (!shader->Create(data))
    {
        delete shader;
        return nullptr;
    }
    return shader;
}

IGfxPipelineState* MockDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    MockGraphicsPipelineState* pso = new MockGraphicsPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* MockDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    MockMeshShadingPipelineState* pso = new MockMeshShadingPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* MockDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    MockComputePipelineState* pso = new MockComputePipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxDescriptor* MockDevice::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    MockShaderResourceView* srv = new MockShaderResourceView(this, resource, desc, name);
    if (!srv->Create())
    {
        delete srv;
        return nullptr;
    }
    return srv;
}

IGfxDescriptor* MockDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    MockUnorderedAccessView* uav = new MockUnorderedAccessView(this, resource, desc, name);
    if (!uav->Create())
    {
        delete uav;
        return nullptr;
    }
    return uav;
}

IGfxDescriptor* MockDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    MockConstantBufferView* cbv = new MockConstantBufferView(this, buffer, desc, name);
    if (!cbv->Create())
    {
        delete cbv;
        return nullptr;
    }
    return cbv;
}

IGfxDescriptor* MockDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    MockSampler* sampler = new MockSampler(this, desc, name);
    if (!sampler->Create())
    {
        delete sampler;
        return nullptr;
    }
    return sampler;
}

IGfxRayTracingBLAS* MockDevice::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    MockRayTracingBLAS* blas = new MockRayTracingBLAS(this, desc, name);
    if (!blas->Create())
    {
        delete blas;
        return nullptr;
    }
    return blas;
}

IGfxRayTracingTLAS* MockDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    MockRayTracingTLAS* tlas = new MockRayTracingTLAS(this, desc, name);
    if (!tlas->Create())
    {
        delete tlas;
        return nullptr;
    }
    return tlas;
}

uint32_t MockDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    uint32_t size = 0;

    for (uint32_t layer = 0; layer < desc.array_size; ++layer)
    {
        for (uint32_t mip = 0; mip < desc.mip_levels; ++mip)
        {
            uint32_t width = eastl::max(desc.width >> mip, 1u);
            uint32_t height = eastl::max(desc.height >> mip, 1u);
            size += GetFormatRowPitch(desc.format, width) * height;
        }
    }

    return size;
}

bool MockDevice::DumpMemoryStats(const eastl::string& file)
{
    return false;
}
