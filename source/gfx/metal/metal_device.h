#pragma once

#include "../gfx_device.h"


class MetalDevice : public IGfxDevice
{
public:
    virtual void BeginFrame() override;
    virtual void EndFrame() override;
    virtual uint64_t GetFrameID() const override;
    virtual void* GetHandle() const override;
    virtual GfxVendor GetVendor() const override;

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
};