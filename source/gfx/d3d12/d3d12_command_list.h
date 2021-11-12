#pragma once

#include "d3d12_header.h"
#include "../i_gfx_command_list.h"

class IGfxDevice;
class IGfxFence;

class D3D12CommandList : public IGfxCommandList
{
public:
	D3D12CommandList(IGfxDevice* pDevice, GfxCommandQueue queue_type, const std::string& name);
	~D3D12CommandList();

	bool Create();

	virtual void* GetHandle() const override { return m_pCommandList; }

	virtual void Begin() override;
	virtual void End() override;
	virtual void Wait(IGfxFence* fence, uint64_t value) override;
	virtual void Signal(IGfxFence* fence, uint64_t value) override;
	virtual void Submit() override;

	virtual void BeginEvent(const std::string& event_name) override;
	virtual void EndEvent() override;

	virtual void CopyBufferToTexture(IGfxTexture* texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* buffer, uint32_t offset) override;
	virtual void CopyBuffer(IGfxBuffer* dst_buffer, uint32_t dst_offset, IGfxBuffer* src_buffer, uint32_t src_offset, uint32_t size) override;
	virtual void WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data) override;
	virtual void CopyTexture(IGfxTexture* dst, IGfxTexture* src) override;

	virtual void ResourceBarrier(IGfxResource* resource, uint32_t sub_resource, GfxResourceState old_state, GfxResourceState new_state) override;
	virtual void UavBarrier(IGfxResource* resource) override;
	virtual void AliasingBarrier(IGfxResource* resource_before, IGfxResource* resource_after) override;

	virtual void BeginRenderPass(const GfxRenderPassDesc& render_pass) override;
	virtual void EndRenderPass() override;
	virtual void SetPipelineState(IGfxPipelineState* state) override;
	virtual void SetStencilReference(uint8_t stencil) override;
	virtual void SetBlendFactor(const float* blend_factor) override;
	virtual void SetIndexBuffer(IGfxBuffer* buffer) override;
	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
	virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
	virtual void SetGraphicsConstants(uint32_t slot, void* data, size_t data_size) override;
	virtual void SetComputeConstants(uint32_t slot, void* data, size_t data_size) override;

	virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1) override;
	virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t index_offset = 0) override;
	virtual void Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;

	virtual void DrawIndirect(IGfxBuffer* buffer, uint32_t offset) override;
	virtual void DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset) override;
	virtual void DispatchIndirect(IGfxBuffer* buffer, uint32_t offset) override;

#if MICROPROFILE_GPU_TIMERS
	virtual struct MicroProfileThreadLogGpu* GetProfileLog() const override;
#endif

private:
	void FlushPendingBarrier();

private:
	GfxCommandQueue m_queueType;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList4* m_pCommandList = nullptr;

	IGfxPipelineState* m_pCurrentPSO = nullptr;

	std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;

#if MICROPROFILE_GPU_TIMERS_D3D12
	struct MicroProfileThreadLogGpu* m_pProfileLog = nullptr;
	int m_nProfileQueue = -1;
#endif
};