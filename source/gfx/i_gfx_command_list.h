#pragma once

#include "i_gfx_resource.h"

class IGfxFence;
class IGfxBuffer;
class IGfxTexture;
class IGfxPipelineState;

class IGfxCommandList : public IGfxResource
{
public:
	virtual ~IGfxCommandList() {}

	virtual void Begin() = 0;
	virtual void End() = 0;
	virtual void Wait(IGfxFence* fence, uint64_t value) = 0;
	virtual void Signal(IGfxFence* fence, uint64_t value) = 0;
	virtual void Submit() = 0;

	virtual void BeginEvent(const std::string& event_name) = 0;
	virtual void EndEvent() = 0;

	virtual void CopyBufferToTexture(IGfxTexture* texture, IGfxBuffer* buffer, uint32_t data_offset, uint32_t data_size) = 0;

	virtual void ResourceBarrier(IGfxResource* resource, uint32_t sub_resource, GfxResourceState old_state, GfxResourceState new_state) = 0;
	virtual void UavBarrier(IGfxResource* resource) = 0;
	virtual void BeginRenderPass(const GfxRenderPassDesc& render_pass) = 0;
	virtual void EndRenderPass() = 0;
	virtual void SetPipelineState(IGfxPipelineState* state) = 0;
	virtual void SetIndexBuffer(IGfxBuffer* buffer) = 0;
	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
	virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
	virtual void SetConstantBuffer(GfxPipelineType type, uint32_t slot, void* data, size_t data_size) = 0;
	virtual void Draw(uint32_t vertex_count, uint32_t instance_count) = 0;
	virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset = 0) = 0;
};