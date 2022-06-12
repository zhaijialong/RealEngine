#include "d3d12_command_list.h"
#include "d3d12_device.h"
#include "d3d12_fence.h"
#include "d3d12_texture.h"
#include "d3d12_buffer.h"
#include "d3d12_pipeline_state.h"
#include "d3d12_heap.h"
#include "d3d12_descriptor.h"
#include "d3d12_rt_blas.h"
#include "d3d12_rt_tlas.h"
#include "pix_runtime.h"
#include "ags.h"
#include "../gfx.h"
#include "utils/assert.h"
#include "utils/profiler.h"

D3D12CommandList::D3D12CommandList(IGfxDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
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

void D3D12CommandList::ResetAllocator()
{
    m_pCommandAllocator->Reset();
}

void D3D12CommandList::Begin()
{
    m_pCurrentPSO = nullptr;
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

void D3D12CommandList::Wait(IGfxFence* fence, uint64_t value)
{
    m_pendingWaits.emplace_back(fence, value);
}

void D3D12CommandList::Signal(IGfxFence* fence, uint64_t value)
{
    m_pendingSignals.emplace_back(fence, value);
}

void D3D12CommandList::Submit()
{
    for (size_t i = 0; i < m_pendingWaits.size(); ++i)
    {
        m_pCommandQueue->Wait((ID3D12Fence*)m_pendingWaits[i].first->GetHandle(), m_pendingWaits[i].second);
    }
    m_pendingWaits.clear();

    ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

#if MICROPROFILE_GPU_TIMERS_D3D12	
    if (m_nProfileQueue != -1)
    {
        MicroProfileGpuSubmit(m_nProfileQueue, MicroProfileGpuEnd(m_pProfileLog));
        MicroProfileThreadLogGpuFree(m_pProfileLog);
    }
#endif

    for (size_t i = 0; i < m_pendingSignals.size(); ++i)
    {
        m_pCommandQueue->Signal((ID3D12Fence*)m_pendingSignals[i].first->GetHandle(), m_pendingSignals[i].second);
    }
    m_pendingSignals.clear();
}

void D3D12CommandList::BeginEvent(const eastl::string& event_name)
{
    pix::BeginEvent(m_pCommandList, event_name.c_str());
    ags::BeginEvent(m_pCommandList, event_name.c_str());
}

void D3D12CommandList::EndEvent()
{
    pix::EndEvent(m_pCommandList);
    ags::EndEvent(m_pCommandList);
}

void D3D12CommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
    FlushPendingBarrier();

    const GfxTextureDesc& desc = dst_texture->GetDesc();

    uint32_t min_width = GetFormatBlockWidth(desc.format);
    uint32_t min_height = GetFormatBlockHeight(desc.format);
    uint32_t w = eastl::max(desc.width >> mip_level, min_width);
    uint32_t h = eastl::max(desc.height >> mip_level, min_height);
    uint32_t d = eastl::max(desc.depth >> mip_level, 1u);

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = (ID3D12Resource*)dst_texture->GetHandle();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = CalcSubresource(desc, mip_level, array_slice);

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = (ID3D12Resource*)src_buffer->GetHandle();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = offset;
    src.PlacedFootprint.Footprint.Format = dxgi_format(desc.format);
    src.PlacedFootprint.Footprint.Width = w;
    src.PlacedFootprint.Footprint.Height = h;
    src.PlacedFootprint.Footprint.Depth = d;
    src.PlacedFootprint.Footprint.RowPitch = dst_texture->GetRowPitch(mip_level);

    m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

void D3D12CommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
    FlushPendingBarrier();

    GfxTextureDesc desc = src_texture->GetDesc();

    uint32_t min_width = GetFormatBlockWidth(desc.format);
    uint32_t min_height = GetFormatBlockHeight(desc.format);
    uint32_t w = eastl::max(desc.width >> mip_level, min_width);
    uint32_t h = eastl::max(desc.height >> mip_level, min_height);
    uint32_t d = eastl::max(desc.depth >> mip_level, 1u);

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = (ID3D12Resource*)dst_buffer->GetHandle();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = dxgi_format(desc.format);
    dst.PlacedFootprint.Footprint.Width = eastl::max(desc.width >> mip_level, 1u);
    dst.PlacedFootprint.Footprint.Height = eastl::max(desc.height >> mip_level, 1u);
    dst.PlacedFootprint.Footprint.Depth = eastl::max(desc.depth >> mip_level, 1u);
    dst.PlacedFootprint.Footprint.RowPitch = src_texture->GetRowPitch(mip_level);

    D3D12_TEXTURE_COPY_LOCATION src;
    src.pResource = (ID3D12Resource*)src_texture->GetHandle();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = CalcSubresource(desc, mip_level, array_slice);

    m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

void D3D12CommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
    FlushPendingBarrier();

    m_pCommandList->CopyBufferRegion((ID3D12Resource*)dst->GetHandle(), dst_offset,
        (ID3D12Resource*)src->GetHandle(), src_offset, size);
}

void D3D12CommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
    FlushPendingBarrier();

    D3D12_TEXTURE_COPY_LOCATION dst_texture;
    dst_texture.pResource = (ID3D12Resource*)dst->GetHandle();
    dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_texture.SubresourceIndex = CalcSubresource(dst->GetDesc(), dst_mip, dst_array);

    D3D12_TEXTURE_COPY_LOCATION src_texture;
    src_texture.pResource = (ID3D12Resource*)src->GetHandle();
    src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_texture.SubresourceIndex = CalcSubresource(src->GetDesc(), src_mip, src_array);

    m_pCommandList->CopyTextureRegion(&dst_texture, 0, 0, 0, &src_texture, nullptr);
}

void D3D12CommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
    FlushPendingBarrier();

    D3D12Descriptor shaderVisibleDescriptor = ((D3D12UnorderedAccessView*)uav)->GetShaderVisibleDescriptor();
    D3D12Descriptor nonShaderVisibleDescriptor = ((D3D12UnorderedAccessView*)uav)->GetNonShaderVisibleDescriptor();

    m_pCommandList->ClearUnorderedAccessViewFloat(shaderVisibleDescriptor.gpu_handle, nonShaderVisibleDescriptor.cpu_handle,
        (ID3D12Resource*)resource->GetHandle(), clear_value, 0, nullptr);
}

void D3D12CommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
    FlushPendingBarrier();

    D3D12Descriptor shaderVisibleDescriptor = ((D3D12UnorderedAccessView*)uav)->GetShaderVisibleDescriptor();
    D3D12Descriptor nonShaderVisibleDescriptor = ((D3D12UnorderedAccessView*)uav)->GetNonShaderVisibleDescriptor();

    m_pCommandList->ClearUnorderedAccessViewUint(shaderVisibleDescriptor.gpu_handle, nonShaderVisibleDescriptor.cpu_handle,
        (ID3D12Resource*)resource->GetHandle(), clear_value, 0, nullptr);
}

void D3D12CommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
    FlushPendingBarrier();

    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter;
    parameter.Dest = buffer->GetGpuAddress() + offset;
    parameter.Value = data;

    m_pCommandList->WriteBufferImmediate(1, &parameter, nullptr);
}

void D3D12CommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
    eastl::vector<D3D12_TILED_RESOURCE_COORDINATE> coordinates;
    eastl::vector<D3D12_TILE_REGION_SIZE> sizes;
    eastl::vector<D3D12_TILE_RANGE_FLAGS> flags;
    eastl::vector<UINT> heapTileOffsets;
    eastl::vector<UINT> tileCounts;

    coordinates.reserve(mapping_count);
    sizes.reserve(mapping_count);
    flags.reserve(mapping_count);
    heapTileOffsets.reserve(mapping_count);
    tileCounts.reserve(mapping_count);

    for (uint32_t i = 0; i < mapping_count; ++i)
    {
        D3D12_TILED_RESOURCE_COORDINATE coordinate;
        coordinate.Subresource = mappings[i].subresource;
        coordinate.X = mappings[i].x;
        coordinate.Y = mappings[i].y;
        coordinate.Z = mappings[i].z;

        if (mappings[i].use_box)
        {
            RE_ASSERT(mappings[i].tile_count == mappings[i].width * mappings[i].height * mappings[i].depth);
        }

        D3D12_TILE_REGION_SIZE size;
        size.UseBox = mappings[i].use_box;
        size.NumTiles = mappings[i].tile_count;
        size.Width = mappings[i].width;
        size.Height = mappings[i].height;
        size.Depth = mappings[i].depth;

        D3D12_TILE_RANGE_FLAGS flag = mappings[i].type == GfxTileMappingType::Map ? D3D12_TILE_RANGE_FLAG_NONE : D3D12_TILE_RANGE_FLAG_NULL;
        UINT tile_count = size.NumTiles;

        coordinates.push_back(coordinate);
        sizes.push_back(size);
        flags.push_back(flag);
        heapTileOffsets.push_back(mappings[i].heap_offset);
        tileCounts.push_back(tile_count);
    }

    m_pCommandQueue->UpdateTileMappings(
        (ID3D12Resource*)texture->GetHandle(),
        mapping_count,
        coordinates.data(),
        sizes.data(),
        ((D3D12Heap*)heap)->GetHeap(),
        mapping_count,
        flags.data(),
        heapTileOffsets.data(),
        tileCounts.data(),
        D3D12_TILE_MAPPING_FLAG_NONE);
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
    barrier.UAV.pResource = resource ? (ID3D12Resource*)resource->GetHandle() : nullptr;

    m_pendingBarriers.push_back(barrier);
}

void D3D12CommandList::AliasingBarrier(IGfxResource* resource_before, IGfxResource* resource_after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barrier.Aliasing.pResourceBefore = (ID3D12Resource*)resource_before->GetHandle();
    barrier.Aliasing.pResourceAfter = (ID3D12Resource*)resource_after->GetHandle();

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

void D3D12CommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
    RE_ASSERT(format == GfxFormat::R16UI || format == GfxFormat::R32UI);

    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = buffer->GetGpuAddress() + offset;
    ibv.SizeInBytes = buffer->GetDesc().size - offset;
    ibv.Format = dxgi_format(format);

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

void D3D12CommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
    RE_ASSERT((slot == 0 && data_size <= sizeof(int32_t) * 5) || (slot >= 1 && slot <= 4));

    if (slot == 0)
    {
        uint32_t consts_count = (uint32_t)data_size / sizeof(uint32_t);
        m_pCommandList->SetGraphicsRoot32BitConstants(0, consts_count, data, 0);
    }
    else
    {
        D3D12_GPU_VIRTUAL_ADDRESS address = ((D3D12Device*)m_pDevice)->AllocateConstantBuffer(data, data_size);
        RE_ASSERT(address);

        m_pCommandList->SetGraphicsRootConstantBufferView(slot, address);
    }
}

void D3D12CommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
    RE_ASSERT((slot == 0 && data_size <= sizeof(int32_t) * 5) || (slot >= 1 && slot <= 4));

    if (slot == 0)
    {
        uint32_t consts_count = (uint32_t)data_size / sizeof(uint32_t);
        m_pCommandList->SetComputeRoot32BitConstants(0, consts_count, data, 0);
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

void D3D12CommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    m_pCommandList->DispatchMesh(group_count_x, group_count_y, group_count_z);
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

void D3D12CommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetDispatchMeshSignature();
    m_pCommandList->ExecuteIndirect(signature, 1, (ID3D12Resource*)buffer->GetHandle(), offset, nullptr, 0);
}

void D3D12CommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetMultiDrawSignature();
    m_pCommandList->ExecuteIndirect(signature, max_count, 
        (ID3D12Resource*)args_buffer->GetHandle(), args_buffer_offset, 
        (ID3D12Resource*)count_buffer->GetHandle(), count_buffer_offset);
}

void D3D12CommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetMultiDrawIndexedSignature();
    m_pCommandList->ExecuteIndirect(signature, max_count,
        (ID3D12Resource*)args_buffer->GetHandle(), args_buffer_offset,
        (ID3D12Resource*)count_buffer->GetHandle(), count_buffer_offset);
}

void D3D12CommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetMultiDispatchSignature();
    m_pCommandList->ExecuteIndirect(signature, max_count,
        (ID3D12Resource*)args_buffer->GetHandle(), args_buffer_offset,
        (ID3D12Resource*)count_buffer->GetHandle(), count_buffer_offset);
}

void D3D12CommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    ID3D12CommandSignature* signature = ((D3D12Device*)m_pDevice)->GetMultiDispatchMeshSignature();
    m_pCommandList->ExecuteIndirect(signature, max_count,
        (ID3D12Resource*)args_buffer->GetHandle(), args_buffer_offset,
        (ID3D12Resource*)count_buffer->GetHandle(), count_buffer_offset);
}

void D3D12CommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
    FlushPendingBarrier();

    m_pCommandList->BuildRaytracingAccelerationStructure(((D3D12RayTracingBLAS*)blas)->GetBuildDesc(), 0, nullptr);
}

void D3D12CommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    FlushPendingBarrier();

    D3D12_RAYTRACING_GEOMETRY_DESC geometry;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc;
    ((D3D12RayTracingBLAS*)blas)->GetUpdateDesc(desc, geometry, vertex_buffer, vertex_buffer_offset);

    m_pCommandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}

void D3D12CommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    FlushPendingBarrier();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    ((D3D12RayTracingTLAS*)tlas)->GetBuildDesc(desc, instances, instance_count);

    m_pCommandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* D3D12CommandList::GetProfileLog() const
{
#if MICROPROFILE_GPU_TIMERS_D3D12
    return m_pProfileLog;
#else
    return nullptr;
#endif
}
#endif

void D3D12CommandList::FlushPendingBarrier()
{
    if (!m_pendingBarriers.empty())
    {
        m_pCommandList->ResourceBarrier((UINT)m_pendingBarriers.size(), m_pendingBarriers.data());
        m_pendingBarriers.clear();
    }
}
