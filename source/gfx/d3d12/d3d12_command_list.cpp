#include "d3d12_command_list.h"
#include "d3d12_device.h"
#include "d3d12_fence.h"
#include "d3d12_texture.h"
#include "d3d12_buffer.h"
#include "d3d12_pipeline_state.h"
#include "pix_runtime.h"
#include "../gfx.h"
#include "utils/assert.h"
#include "utils/profiler.h"

D3D12CommandList::D3D12CommandList(IGfxDevice* pDevice, GfxCommandQueue queue_type, const std::string& name)
{
	m_pDevice = pDevice;
	m_name = name;
	m_queueType = queue_type;
}

D3D12CommandList::~D3D12CommandList()
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;
	pDevice->Delete(m_pCommandAllocator);
	pDevice->Delete(m_pCommandList);
}

bool D3D12CommandList::Create()
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;
	D3D12_COMMAND_LIST_TYPE type;

	switch (m_queueType)
	{
	case GfxCommandQueue::Graphics:
		type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		m_pCommandQueue = pDevice->GetGraphicsQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
		m_nProfileQueue = pDevice->GetProfileGraphicsQueue();
#endif
		break;
	case GfxCommandQueue::Compute:
		type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		m_pCommandQueue = pDevice->GetComputeQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
		m_nProfileQueue = pDevice->GetProfileComputeQueue();
#endif
		break;
	case GfxCommandQueue::Copy:
		type = D3D12_COMMAND_LIST_TYPE_COPY;
		m_pCommandQueue = pDevice->GetCopyQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
		m_nProfileQueue = pDevice->GetProfileCopyQueue();
#endif
		break;
	default:
		break;
	}

	ID3D12Device* pD3D12Device = (ID3D12Device*)pDevice->GetHandle();
	HRESULT hr = pD3D12Device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_pCommandAllocator));
	if (FAILED(hr))
	{
		return false;
	}
	m_pCommandAllocator->SetName(string_to_wstring(m_name + " allocator").c_str());

	hr = pD3D12Device->CreateCommandList(0, type, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
	if (FAILED(hr))
	{
		return false;
	}

	m_pCommandList->SetName(string_to_wstring(m_name).c_str());
	m_pCommandList->Close();

	return true;
}

void D3D12CommandList::Begin()
{
	m_pCurrentPSO = nullptr;

	m_pCommandAllocator->Reset();
	m_pCommandList->Reset(m_pCommandAllocator, nullptr);

	if (m_queueType == GfxCommandQueue::Graphics || m_queueType == GfxCommandQueue::Compute)
	{
		D3D12Device* pDevice = (D3D12Device*)m_pDevice;
		ID3D12DescriptorHeap* heaps[2] = { pDevice->GetResourceDescriptorHeap(), pDevice->GetSamplerDescriptorHeap() };
		m_pCommandList->SetDescriptorHeaps(2, heaps);

		ID3D12RootSignature* pRootSignature = pDevice->GetRootSignature();
		m_pCommandList->SetComputeRootSignature(pRootSignature);

		if(m_queueType == GfxCommandQueue::Graphics)
		{
			m_pCommandList->SetGraphicsRootSignature(pRootSignature);
		}
	}

#if MICROPROFILE_GPU_TIMERS_D3D12
	if (m_nProfileQueue != -1)
	{
		m_pProfileLog = MicroProfileThreadLogGpuAlloc();
		MicroProfileGpuBegin(m_pCommandList, m_pProfileLog);
	}
#endif
}

void D3D12CommandList::End()
{
	FlushPendingBarrier();

	m_pCommandList->Close();
}

void D3D12CommandList::BeginEvent(const std::string& event_name)
{
	pix::BeginEvent(m_pCommandList, event_name.c_str());
}

void D3D12CommandList::EndEvent()
{
	pix::EndEvent(m_pCommandList);
}

void D3D12CommandList::CopyBufferToTexture(IGfxTexture* texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* buffer, uint32_t offset)
{
	const GfxTextureDesc& desc = texture->GetDesc();
	uint32_t subresource = array_slice * desc.mip_levels + mip_level;

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = (ID3D12Resource*)texture->GetHandle();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = subresource;

	uint32_t min_width = GetFormatBlockWidth(desc.format);
	uint32_t min_height = GetFormatBlockHeight(desc.format);
	uint32_t w = max(desc.width >> mip_level, min_width);
	uint32_t h = max(desc.height >> mip_level, min_height);
	uint32_t d = max(desc.depth >> mip_level, 1);

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = (ID3D12Resource*)buffer->GetHandle();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = offset;
	src.PlacedFootprint.Footprint.Format = dxgi_format(desc.format);
	src.PlacedFootprint.Footprint.Width = w;
	src.PlacedFootprint.Footprint.Height = h;
	src.PlacedFootprint.Footprint.Depth = d;
	src.PlacedFootprint.Footprint.RowPitch = texture->GetRowPitch(mip_level);// GetFormatRowPitch(desc.format, w);

	FlushPendingBarrier();

	m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

void D3D12CommandList::CopyBuffer(IGfxBuffer* dst_buffer, uint32_t dst_offset, IGfxBuffer* src_buffer, uint32_t src_offset, uint32_t size)
{
	FlushPendingBarrier();

	m_pCommandList->CopyBufferRegion((ID3D12Resource*)dst_buffer->GetHandle(), dst_offset,
		(ID3D12Resource*)src_buffer->GetHandle(), src_offset, size);
}

void D3D12CommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
	FlushPendingBarrier();

	D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter;
	parameter.Dest = buffer->GetGpuAddress() + offset;
	parameter.Value = data;

	m_pCommandList->WriteBufferImmediate(1, &parameter, nullptr);
}

void D3D12CommandList::CopyTexture(IGfxTexture* dst, IGfxTexture* src)
{
	FlushPendingBarrier();

	D3D12_TEXTURE_COPY_LOCATION dst_texture;
	dst_texture.pResource = (ID3D12Resource*)dst->GetHandle();
	dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_texture.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src_texture;
	src_texture.pResource = (ID3D12Resource*)src->GetHandle();
	src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_texture.SubresourceIndex = 0;

	m_pCommandList->CopyTextureRegion(&dst_texture, 0, 0, 0, &src_texture, nullptr);
}

void D3D12CommandList::Wait(IGfxFence* fence, uint64_t value)
{
	m_pCommandQueue->Wait((ID3D12Fence*)fence->GetHandle(), value);
}

void D3D12CommandList::Signal(IGfxFence* fence, uint64_t value)
{
	m_pCommandQueue->Signal((ID3D12Fence*)fence->GetHandle(), value);
}

void D3D12CommandList::Submit()
{
	ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

#if MICROPROFILE_GPU_TIMERS_D3D12	
	if (m_nProfileQueue != -1)
	{
		MicroProfileGpuSubmit(m_nProfileQueue, MicroProfileGpuEnd(m_pProfileLog));
		MicroProfileThreadLogGpuFree(m_pProfileLog);
	}
#endif
}

void D3D12CommandList::ResourceBarrier(IGfxResource* resource, uint32_t sub_resource, GfxResourceState old_state, GfxResourceState new_state)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = (ID3D12Resource*)resource->GetHandle();
	barrier.Transition.Subresource = sub_resource;
	barrier.Transition.StateBefore = d3d12_resource_state(old_state);
	barrier.Transition.StateAfter = d3d12_resource_state(new_state);

	m_pendingBarriers.push_back(barrier);
}

void D3D12CommandList::UavBarrier(IGfxResource* resource)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = (ID3D12Resource*)resource->GetHandle();

	m_pendingBarriers.push_back(barrier);
}

void D3D12CommandList::AliasingBarrier(IGfxResource* resource_before, IGfxResource* resource_after)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
	barrier.Aliasing.pResourceBefore = (ID3D12Resource*)resource_before;
	barrier.Aliasing.pResourceAfter = (ID3D12Resource*)resource_after;

	m_pendingBarriers.push_back(barrier);
}

void D3D12CommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
	FlushPendingBarrier();

	D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc[8] = {};
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc = {};

	uint32_t width = 0;
	uint32_t height = 0;

	unsigned int rt_count = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (render_pass.color[i].texture == nullptr)
		{
			continue;
		}

		if (width == 0)
		{
			width = render_pass.color[i].texture->GetDesc().width;
		}

		if (height == 0)
		{
			height = render_pass.color[i].texture->GetDesc().height;
		}

		RE_ASSERT(width == render_pass.color[i].texture->GetDesc().width);
		RE_ASSERT(height == render_pass.color[i].texture->GetDesc().height);

		rtDesc[i].cpuDescriptor = ((D3D12Texture*)render_pass.color[i].texture)->GetRTV(render_pass.color[i].mip_slice, render_pass.color[i].array_slice);
		rtDesc[i].BeginningAccess.Type = d3d12_render_pass_loadop(render_pass.color[i].load_op);
		rtDesc[i].BeginningAccess.Clear.ClearValue.Format = dxgi_format(render_pass.color[i].texture->GetDesc().format);
		memcpy(rtDesc[i].BeginningAccess.Clear.ClearValue.Color, render_pass.color[i].clear_color, sizeof(float) * 4);
		rtDesc[i].EndingAccess.Type = d3d12_render_pass_storeop(render_pass.color[i].store_op);

		++rt_count;
	}

	if (render_pass.depth.texture != nullptr)
	{
		if (width == 0)
		{
			width = render_pass.depth.texture->GetDesc().width;
		}

		if (height == 0)
		{
			height = render_pass.depth.texture->GetDesc().height;
		}

		RE_ASSERT(width == render_pass.depth.texture->GetDesc().width);
		RE_ASSERT(height == render_pass.depth.texture->GetDesc().height);

		dsDesc.cpuDescriptor = ((D3D12Texture*)render_pass.depth.texture)->GetDSV(render_pass.depth.mip_slice, render_pass.depth.array_slice);
		dsDesc.DepthBeginningAccess.Type = d3d12_render_pass_loadop(render_pass.depth.load_op);
		dsDesc.DepthBeginningAccess.Clear.ClearValue.Format = dxgi_format(render_pass.depth.texture->GetDesc().format);
		dsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = render_pass.depth.clear_depth;
		dsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = render_pass.depth.clear_stencil;

		dsDesc.StencilBeginningAccess.Type = d3d12_render_pass_loadop(render_pass.depth.stencil_load_op);
		dsDesc.StencilBeginningAccess.Clear.ClearValue.Format = dxgi_format(render_pass.depth.texture->GetDesc().format);
		dsDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Depth = render_pass.depth.clear_depth;
		dsDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = render_pass.depth.clear_stencil;

		dsDesc.DepthEndingAccess.Type = d3d12_render_pass_storeop(render_pass.depth.store_op);
		dsDesc.StencilEndingAccess.Type = d3d12_render_pass_storeop(render_pass.depth.stencil_store_op);
	}

	m_pCommandList->BeginRenderPass(rt_count, rtDesc, 
		render_pass.depth.texture != nullptr ? &dsDesc : nullptr,
		D3D12_RENDER_PASS_FLAG_NONE);

	SetViewport(0, 0, width, height);
}

void D3D12CommandList::EndRenderPass()
{
	m_pCommandList->EndRenderPass();
}

void D3D12CommandList::SetPipelineState(IGfxPipelineState* state)
{
	if (m_pCurrentPSO != state)
	{
		m_pCurrentPSO = state;

		m_pCommandList->SetPipelineState((ID3D12PipelineState*)state->GetHandle());

		if (state->GetType() == GfxPipelineType::Graphics)
		{
			m_pCommandList->IASetPrimitiveTopology(((D3D12GraphicsPipelineState*)state)->GetPrimitiveTopology());
		}
	}
}

void D3D12CommandList::SetStencilReference(uint8_t stencil)
{
	m_pCommandList->OMSetStencilRef(stencil);
}

void D3D12CommandList::SetBlendFactor(const float* blend_factor)
{
	m_pCommandList->OMSetBlendFactor(blend_factor);
}

void D3D12CommandList::SetIndexBuffer(IGfxBuffer* buffer)
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = buffer->GetGpuAddress();
	ibv.SizeInBytes = buffer->GetDesc().size;
	ibv.Format = dxgi_format(buffer->GetDesc().format);

	m_pCommandList->IASetIndexBuffer(&ibv);
}

void D3D12CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	D3D12_VIEWPORT vp = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
	m_pCommandList->RSSetViewports(1, &vp);

	SetScissorRect(x, y, width, height);
}

void D3D12CommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	D3D12_RECT rect = { (LONG)x, (LONG)y, LONG(x + width), LONG(y + height) };
	m_pCommandList->RSSetScissorRects(1, &rect);
}

void D3D12CommandList::SetGraphicsConstants(uint32_t slot, void* data, size_t data_size)
{
	RE_ASSERT((slot == 0 && data_size == 16) || (slot >= 1 && slot <= 4));

	if (slot == 0)
	{
		m_pCommandList->SetGraphicsRoot32BitConstants(0, 4, data, 0);
	}
	else
	{
		D3D12_GPU_VIRTUAL_ADDRESS address = ((D3D12Device*)m_pDevice)->AllocateConstantBuffer(data, data_size);
		RE_ASSERT(address);

		m_pCommandList->SetGraphicsRootConstantBufferView(slot, address);
	}
}

void D3D12CommandList::SetComputeConstants(uint32_t slot, void* data, size_t data_size)
{
	RE_ASSERT((slot == 0 && data_size == 16) || (slot >= 1 && slot <= 4));

	if (slot == 0)
	{
		m_pCommandList->SetComputeRoot32BitConstants(0, 4, data, 0);
	}
	else
	{
		D3D12_GPU_VIRTUAL_ADDRESS address = ((D3D12Device*)m_pDevice)->AllocateConstantBuffer(data, data_size);
		RE_ASSERT(address);

		m_pCommandList->SetComputeRootConstantBufferView(slot, address);
	}
}

void D3D12CommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
	m_pCommandList->DrawInstanced(vertex_count, instance_count, 0, 0);
}

void D3D12CommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
	m_pCommandList->DrawIndexedInstanced(index_count, instance_count, index_offset, 0, 0);
}

void D3D12CommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	FlushPendingBarrier();

	m_pCommandList->Dispatch(group_count_x, group_count_y, group_count_z);
}

void D3D12CommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
	ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetDrawSignature();
	m_pCommandList->ExecuteIndirect(signature, 1, (ID3D12Resource*)buffer->GetHandle(), offset, nullptr, 0);
}

void D3D12CommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
	ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetDrawIndexedSignature();
	m_pCommandList->ExecuteIndirect(signature, 1, (ID3D12Resource*)buffer->GetHandle(), offset, nullptr, 0);
}

void D3D12CommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
	FlushPendingBarrier();

	ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetDispatchSignature();
	m_pCommandList->ExecuteIndirect(signature, 1, (ID3D12Resource*)buffer->GetHandle(), offset, nullptr, 0);
}

void D3D12CommandList::FlushPendingBarrier()
{
	if (!m_pendingBarriers.empty())
	{
		m_pCommandList->ResourceBarrier((UINT)m_pendingBarriers.size(), m_pendingBarriers.data());
		m_pendingBarriers.clear();
	}
}
