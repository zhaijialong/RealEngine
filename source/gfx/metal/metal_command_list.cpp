#include "metal_command_list.h"
#include "metal_device.h"
#include "metal_swapchain.h"
#include "metal_fence.h"

MetalCommandList::MetalCommandList(MetalDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_queueType = queue_type;
}

MetalCommandList::~MetalCommandList()
{
}

bool MetalCommandList::Create()
{
    return true;
}

void* MetalCommandList::GetHandle() const
{
    return nullptr;
}

void MetalCommandList::ResetAllocator()
{
}

void MetalCommandList::Begin()
{
    MTL::CommandQueue* queue = ((MetalDevice*)m_pDevice)->GetQueue();
    
    m_pCommandBuffer = queue->commandBuffer();
}

void MetalCommandList::End()
{
}

void MetalCommandList::Wait(IGfxFence* fence, uint64_t value)
{
    m_pCommandBuffer->encodeWait((const MTL::Event*)fence->GetHandle(), value);
}

void MetalCommandList::Signal(IGfxFence* fence, uint64_t value)
{
    m_pCommandBuffer->encodeSignalEvent((const MTL::Event*)fence->GetHandle(), value);
}

void MetalCommandList::Present(IGfxSwapchain* swapchain)
{
    m_pCommandBuffer->presentDrawable(((MetalSwapchain*)swapchain)->GetDrawable());
}

void MetalCommandList::Submit()
{
    m_pCommandBuffer->commit();
    m_pCommandBuffer = nullptr;
}

void MetalCommandList::ResetState()
{
}

void MetalCommandList::BeginProfiling()
{
}

void MetalCommandList::EndProfiling()
{
}

void MetalCommandList::BeginEvent(const eastl::string& event_name)
{
}

void MetalCommandList::EndEvent()
{
}

void MetalCommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
}

void MetalCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
}

void MetalCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
}

void MetalCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
}

void MetalCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
}

void MetalCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
}

void MetalCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
}

void MetalCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
}

void MetalCommandList::TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MetalCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MetalCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MetalCommandList::FlushBarriers()
{
}

void MetalCommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
}

void MetalCommandList::EndRenderPass()
{
}

void MetalCommandList::SetPipelineState(IGfxPipelineState* state)
{
}

void MetalCommandList::SetStencilReference(uint8_t stencil)
{
}

void MetalCommandList::SetBlendFactor(const float* blend_factor)
{
}

void MetalCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
}

void MetalCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void MetalCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void MetalCommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void MetalCommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void MetalCommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
}

void MetalCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
}

void MetalCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void MetalCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void MetalCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MetalCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MetalCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MetalCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MetalCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MetalCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MetalCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MetalCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MetalCommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
}

void MetalCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
}

void MetalCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* MetalCommandList::GetProfileLog() const
{
    return nullptr;
}
#endif
