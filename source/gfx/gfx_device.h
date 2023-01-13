#pragma once

#include "gfx_define.h"

class IGfxResource;
class IGfxBuffer;
class IGfxTexture;
class IGfxFence;
class IGfxSwapchain;
class IGfxCommandList;
class IGfxShader;
class IGfxPipelineState;
class IGfxDescriptor;
class IGfxHeap;
class IGfxRayTracingBLAS;
class IGfxRayTracingTLAS;

class IGfxDevice
{
public:
    virtual ~IGfxDevice() {}

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual uint64_t GetFrameID() const = 0;
    virtual void* GetHandle() const = 0;
    virtual GfxVendor GetVendor() const = 0;

    virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name) = 0;
    virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name) = 0;
    virtual IGfxFence* CreateFence(const eastl::string& name) = 0;
    virtual IGfxHeap* CreateHeap(const GfxHeapDesc& desc, const eastl::string& name) = 0;
    virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name) = 0;
    virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const eastl::string& name) = 0;
    virtual IGfxShader* CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name) = 0;
    virtual IGfxPipelineState* CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name) = 0;
    virtual IGfxPipelineState* CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name) = 0;
    virtual IGfxPipelineState* CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name) = 0;
    virtual IGfxDescriptor* CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name) = 0;
    virtual IGfxDescriptor* CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name) = 0;
    virtual IGfxDescriptor* CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name) = 0;
    virtual IGfxDescriptor* CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name) = 0;
    virtual IGfxRayTracingBLAS* CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name) = 0;
    virtual IGfxRayTracingTLAS* CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name) = 0;

    virtual uint32_t GetAllocationSize(const GfxTextureDesc& desc) = 0;
    virtual bool DumpMemoryStats(const eastl::string& file) = 0;
};