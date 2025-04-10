#include "mock_command_list.h"
#include "mock_device.h"
#include "mock_swapchain.h"

MockCommandList::MockCommandList(MockDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_queueType = queue_type;
}

MockCommandList::~MockCommandList()
{
}

bool MockCommandList::Create()
{
    return true;
}

void* MockCommandList::GetHandle() const
{
    return nullptr;
}

void MockCommandList::ResetAllocator()
{
}

void MockCommandList::Begin()
{
}

void MockCommandList::End()
{
}

void MockCommandList::Wait(IGfxFence* fence, uint64_t value)
{
}

void MockCommandList::Signal(IGfxFence* fence, uint64_t value)
{
}

void MockCommandList::Present(IGfxSwapchain* swapchain)
{
    ((MockSwapchain*)swapchain)->Present();
}

void MockCommandList::Submit()
{
}

void MockCommandList::ResetState()
{
}

void MockCommandList::BeginEvent(const eastl::string& event_name)
{
}

void MockCommandList::EndEvent()
{
}

void MockCommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
}

void MockCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
}

void MockCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
}

void MockCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
}

void MockCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
}

void MockCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
}

void MockCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
}

void MockCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
}

void MockCommandList::TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MockCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MockCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void MockCommandList::FlushBarriers()
{
}

void MockCommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
}

void MockCommandList::EndRenderPass()
{
}

void MockCommandList::SetPipelineState(IGfxPipelineState* state)
{
}

void MockCommandList::SetStencilReference(uint8_t stencil)
{
}

void MockCommandList::SetBlendFactor(const float* blend_factor)
{
}

void MockCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
}

void MockCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void MockCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void MockCommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void MockCommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void MockCommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
}

void MockCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
}

void MockCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void MockCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void MockCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MockCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MockCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MockCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MockCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MockCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MockCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MockCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MockCommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
}

void MockCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
}

void MockCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* MockCommandList::GetProfileLog() const
{
    return nullptr;
}
#endif
