#include "vulkan_command_list.h"
#include "vulkan_device.h"
#include "utils/profiler.h"

VulkanCommandList::VulkanCommandList(VulkanDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_queueType = queue_type;
}

VulkanCommandList::~VulkanCommandList()
{
}

bool VulkanCommandList::Create()
{
    return false;
}

void VulkanCommandList::ResetAllocator()
{
}

void VulkanCommandList::Begin()
{
}

void VulkanCommandList::End()
{
}

void VulkanCommandList::Wait(IGfxFence* fence, uint64_t value)
{
}

void VulkanCommandList::Signal(IGfxFence* fence, uint64_t value)
{
}

void VulkanCommandList::Submit()
{
}

void VulkanCommandList::ClearState()
{
}

void VulkanCommandList::BeginProfiling()
{
}

void VulkanCommandList::EndProfiling()
{
}

void VulkanCommandList::BeginEvent(const eastl::string& event_name)
{
}

void VulkanCommandList::EndEvent()
{
}

void VulkanCommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
}

void VulkanCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
}

void VulkanCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
}

void VulkanCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
}

void VulkanCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
}

void VulkanCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
}

void VulkanCommandList::TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void VulkanCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void VulkanCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void VulkanCommandList::FlushBarriers()
{
}

void VulkanCommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
}

void VulkanCommandList::EndRenderPass()
{
}

void VulkanCommandList::SetPipelineState(IGfxPipelineState* state)
{
}

void VulkanCommandList::SetStencilReference(uint8_t stencil)
{
}

void VulkanCommandList::SetBlendFactor(const float* blend_factor)
{
}

void VulkanCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
}

void VulkanCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void VulkanCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void VulkanCommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void VulkanCommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void VulkanCommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
}

void VulkanCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
}

void VulkanCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void VulkanCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void VulkanCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void VulkanCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void VulkanCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void VulkanCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void VulkanCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void VulkanCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void VulkanCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void VulkanCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void VulkanCommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
}

void VulkanCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
}

void VulkanCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* VulkanCommandList::GetProfileLog() const
{
    return nullptr;
}
#endif
