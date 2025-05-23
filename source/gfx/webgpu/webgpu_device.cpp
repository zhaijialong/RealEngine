#include "webgpu_device.h"
#include "webgpu_buffer.h"
#include "webgpu_texture.h"
#include "webgpu_fence.h"
#include "webgpu_swapchain.h"
#include "webgpu_command_list.h"
#include "webgpu_shader.h"
#include "webgpu_pipeline_state.h"
#include "webgpu_descriptor.h"
#include "webgpu_heap.h"
#include "webgpu_rt_blas.h"
#include "webgpu_rt_tlas.h"
#include "../gfx.h"

#pragma comment(lib, "wgpu_native.dll.lib")

WebGPUDevice::WebGPUDevice(const GfxDeviceDesc& desc)
{
    m_desc = desc;
}

WebGPUDevice::~WebGPUDevice()
{
}

bool WebGPUDevice::Create()
{
    return true;
}

void* WebGPUDevice::GetHandle() const
{
    return nullptr;
}

void WebGPUDevice::BeginFrame()
{
}

void WebGPUDevice::EndFrame()
{
    ++m_frameID;
}

IGfxSwapchain* WebGPUDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    WebGPUSwapchain* swapchain = new WebGPUSwapchain(this, desc, name);
    if (!swapchain->Create())
    {
        delete swapchain;
        return nullptr;
    }
    return swapchain;
}

IGfxCommandList* WebGPUDevice::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    WebGPUCommandList* commandList = new WebGPUCommandList(this, queue_type, name);
    if (!commandList->Create())
    {
        delete commandList;
        return nullptr;
    }
    return commandList;
}

IGfxFence* WebGPUDevice::CreateFence(const eastl::string& name)
{
    WebGPUFence* fence = new WebGPUFence(this, name);
    if (!fence->Create())
    {
        delete fence;
        return nullptr;
    }
    return fence;
}

IGfxHeap* WebGPUDevice::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    WebGPUHeap* heap = new WebGPUHeap(this, desc, name);
    if (!heap->Create())
    {
        delete heap;
        return nullptr;
    }
    return heap;
}

IGfxBuffer* WebGPUDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    WebGPUBuffer* buffer = new WebGPUBuffer(this, desc, name);
    if (!buffer->Create())
    {
        delete buffer;
        return nullptr;
    }
    return buffer;
}

IGfxTexture* WebGPUDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    WebGPUTexture* texture = new WebGPUTexture(this, desc, name);
    if (!texture->Create())
    {
        delete texture;
        return nullptr;
    }
    return texture;
}

IGfxShader* WebGPUDevice::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    WebGPUShader* shader = new WebGPUShader(this, desc, name);
    if (!shader->Create(data))
    {
        delete shader;
        return nullptr;
    }
    return shader;
}

IGfxPipelineState* WebGPUDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    WebGPUGraphicsPipelineState* pso = new WebGPUGraphicsPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* WebGPUDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    WebGPUMeshShadingPipelineState* pso = new WebGPUMeshShadingPipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxPipelineState* WebGPUDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    WebGPUComputePipelineState* pso = new WebGPUComputePipelineState(this, desc, name);
    if (!pso->Create())
    {
        delete pso;
        return nullptr;
    }
    return pso;
}

IGfxDescriptor* WebGPUDevice::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    WebGPUShaderResourceView* srv = new WebGPUShaderResourceView(this, resource, desc, name);
    if (!srv->Create())
    {
        delete srv;
        return nullptr;
    }
    return srv;
}

IGfxDescriptor* WebGPUDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    WebGPUUnorderedAccessView* uav = new WebGPUUnorderedAccessView(this, resource, desc, name);
    if (!uav->Create())
    {
        delete uav;
        return nullptr;
    }
    return uav;
}

IGfxDescriptor* WebGPUDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    WebGPUConstantBufferView* cbv = new WebGPUConstantBufferView(this, buffer, desc, name);
    if (!cbv->Create())
    {
        delete cbv;
        return nullptr;
    }
    return cbv;
}

IGfxDescriptor* WebGPUDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    WebGPUSampler* sampler = new WebGPUSampler(this, desc, name);
    if (!sampler->Create())
    {
        delete sampler;
        return nullptr;
    }
    return sampler;
}

IGfxRayTracingBLAS* WebGPUDevice::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    WebGPURayTracingBLAS* blas = new WebGPURayTracingBLAS(this, desc, name);
    if (!blas->Create())
    {
        delete blas;
        return nullptr;
    }
    return blas;
}

IGfxRayTracingTLAS* WebGPUDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    WebGPURayTracingTLAS* tlas = new WebGPURayTracingTLAS(this, desc, name);
    if (!tlas->Create())
    {
        delete tlas;
        return nullptr;
    }
    return tlas;
}

uint32_t WebGPUDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    uint32_t size = 0;

    uint32_t min_width = GetFormatBlockWidth(desc.format);
    uint32_t min_height = GetFormatBlockHeight(desc.format);

    for (uint32_t layer = 0; layer < desc.array_size; ++layer)
    {
        for (uint32_t mip = 0; mip < desc.mip_levels; ++mip)
        {
            uint32_t width = eastl::max(desc.width >> mip, min_width);
            uint32_t height = eastl::max(desc.height >> mip, min_height);
            uint32_t depth = eastl::max(desc.depth >> mip, 1u);

            size += GetFormatRowPitch(desc.format, width) * height * depth;
        }
    }

    return size;
}

bool WebGPUDevice::DumpMemoryStats(const eastl::string& file)
{
    return false;
}
