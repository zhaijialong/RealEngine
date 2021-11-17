#pragma once

#include "gfx_define.h"
#include <string>
#include <vector>

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

class IGfxDevice
{
public:
	virtual ~IGfxDevice() {}

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual uint64_t GetFrameID() const = 0;
	virtual void* GetHandle() const = 0;

	virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, const std::string& name) = 0;
	virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const std::string& name) = 0;
	virtual IGfxFence* CreateFence(const std::string& name) = 0;
	virtual IGfxHeap* CreateHeap(const GfxHeapDesc& desc, const std::string& name) = 0;
	virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const std::string& name) = 0;
	virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, IGfxHeap* heap, uint32_t offset, const std::string& name) = 0;
	virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const std::string& name) = 0;
	virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, IGfxHeap* heap, uint32_t offset, const std::string& name) = 0;
	virtual IGfxShader* CreateShader(const GfxShaderDesc& desc, const std::vector<uint8_t>& data, const std::string& name) = 0;
	virtual IGfxPipelineState* CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name) = 0;
	virtual IGfxPipelineState* CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const std::string& name) = 0;
	virtual IGfxPipelineState* CreateComputePipelineState(const GfxComputePipelineDesc& desc, const std::string& name) = 0;
	virtual IGfxDescriptor* CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const std::string& name) = 0;
	virtual IGfxDescriptor* CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const std::string& name) = 0;
	virtual IGfxDescriptor* CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const std::string& name) = 0;
	virtual IGfxDescriptor* CreateSampler(const GfxSamplerDesc& desc, const std::string& name) = 0;

	virtual bool DumpMemoryStats(const std::string& file) = 0;
};