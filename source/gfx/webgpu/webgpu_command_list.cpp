#include "webgpu_command_list.h"
#include "webgpu_device.h"
#include "webgpu_swapchain.h"

WebGPUCommandList::WebGPUCommandList(WebGPUDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_queueType = queue_type;
}

WebGPUCommandList::~WebGPUCommandList()
{
}

bool WebGPUCommandList::Create()
{
    return true;
}

void* WebGPUCommandList::GetHandle() const
{
    return nullptr;
}

void WebGPUCommandList::ResetAllocator()
{
}

void WebGPUCommandList::Begin()
{
}

void WebGPUCommandList::End()
{
}

void WebGPUCommandList::Wait(IGfxFence* fence, uint64_t value)
{
}

void WebGPUCommandList::Signal(IGfxFence* fence, uint64_t value)
{
}

void WebGPUCommandList::Present(IGfxSwapchain* swapchain)
{
    ((WebGPUSwapchain*)swapchain)->Present();
}

void WebGPUCommandList::Submit()
{
}

void WebGPUCommandList::ResetState()
{
}

void WebGPUCommandList::BeginEvent(const eastl::string& event_name, const eastl::string& file, const eastl::string& function, uint32_t line)
{
}

void WebGPUCommandList::EndEvent()
{
}

void WebGPUCommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
}

void WebGPUCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
}

void WebGPUCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
}

void WebGPUCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
}

void WebGPUCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
}

void WebGPUCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
}

void WebGPUCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
}

void WebGPUCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
}

void WebGPUCommandList::TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void WebGPUCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void WebGPUCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void WebGPUCommandList::FlushBarriers()
{
}

void WebGPUCommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
}

void WebGPUCommandList::EndRenderPass()
{
}

void WebGPUCommandList::SetPipelineState(IGfxPipelineState* state)
{
}

void WebGPUCommandList::SetStencilReference(uint8_t stencil)
{
}

void WebGPUCommandList::SetBlendFactor(const float* blend_factor)
{
}

void WebGPUCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
}

void WebGPUCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void WebGPUCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
}

void WebGPUCommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void WebGPUCommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
}

void WebGPUCommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
}

void WebGPUCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
}

void WebGPUCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void WebGPUCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void WebGPUCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void WebGPUCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void WebGPUCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void WebGPUCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void WebGPUCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void WebGPUCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void WebGPUCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void WebGPUCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void WebGPUCommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
}

void WebGPUCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
}

void WebGPUCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
}

