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

	virtual void BeginEvent(const std::string& event_name) override;
	virtual void EndEvent() override;

	virtual void Wait(IGfxFence* fence, uint64_t value) override;
	virtual void Signal(IGfxFence* fence, uint64_t value) override;
	virtual void Submit() override;

	virtual void ResourceBarrier(IGfxResource* resource, uint32_t sub_resource, GfxResourceState old_state, GfxResourceState new_state) override;
	virtual void UavBarrier(IGfxResource* resource) override;

	virtual void BeginRenderPass(const GfxRenderPassDesc& render_pass) override;
	virtual void EndRenderPass() override;

	virtual void SetPipelineState(IGfxPipelineState* state) override;
	virtual void SetIndexBuffer(IGfxBuffer* buffer) override;
	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
	virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

	virtual void Draw(uint32_t vertex_count, uint32_t instance_count) override;
	virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count) override;

private:
	void FlushPendingBarrier();

private:
	GfxCommandQueue m_queueType;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList4* m_pCommandList = nullptr;

	IGfxPipelineState* m_pCurrentPSO = nullptr;

	struct FenceOp
	{
		IGfxFence* fence;
		uint64_t value;
	};
	std::vector<FenceOp> m_fenceWaits;
	std::vector<FenceOp> m_fenceSignals;

	std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
};