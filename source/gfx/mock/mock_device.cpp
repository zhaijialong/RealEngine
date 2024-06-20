#include "mock_device.h"

MockDevice::MockDevice(const GfxDeviceDesc& desc)
{

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
}

IGfxSwapchain* MockDevice::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxCommandList* MockDevice::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    return nullptr;
}

IGfxFence* MockDevice::CreateFence(const eastl::string& name)
{
    return nullptr;
}

IGfxHeap* MockDevice::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxBuffer* MockDevice::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxTexture* MockDevice::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxShader* MockDevice::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* MockDevice::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* MockDevice::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxPipelineState* MockDevice::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* MockDevice::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* MockDevice::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* MockDevice::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxDescriptor* MockDevice::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxRayTracingBLAS* MockDevice::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    return nullptr;
}

IGfxRayTracingTLAS* MockDevice::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    return nullptr;
}

uint32_t MockDevice::GetAllocationSize(const GfxTextureDesc& desc)
{
    return 0;
}

bool MockDevice::DumpMemoryStats(const eastl::string& file)
{
    return false;
}
