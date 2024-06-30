#include "metal_command_list.h"
#include "metal_device.h"
#include "metal_swapchain.h"
#include "metal_fence.h"
#include "metal_texture.h"
#include "metal_utils.h"
#include "utils/assert.h"
#include "../gfx.h"

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
    // nop here
    return true;
}

void MetalCommandList::ResetAllocator()
{
}

void MetalCommandList::Begin()
{
    MTL::CommandQueue* queue = ((MetalDevice*)m_pDevice)->GetQueue();
    
    m_pCommandBuffer = queue->commandBuffer();
    
    ResetState();
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
    m_pIndexBuffer = nullptr;
    m_indexBufferOffset = 0;
    m_indexType = MTL::IndexTypeUInt16;
}

void MetalCommandList::BeginProfiling()
{
}

void MetalCommandList::EndProfiling()
{
}

void MetalCommandList::BeginEvent(const eastl::string& event_name)
{
    NS::String* label = NS::String::alloc()->init(event_name.c_str(), NS::StringEncoding::UTF8StringEncoding);
    m_pCommandBuffer->pushDebugGroup(label);
    label->release();
}

void MetalCommandList::EndEvent()
{
    m_pCommandBuffer->popDebugGroup();
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
    MTL::RenderPassDescriptor* descriptor = MTL::RenderPassDescriptor::alloc()->init();

    MTL::RenderPassColorAttachmentDescriptorArray* colorAttachements = descriptor->colorAttachments();
    
    for (uint32_t i = 0; i < 8; ++i)
    {
        if (render_pass.color[i].texture == nullptr)
        {
            continue;
        }

        colorAttachements->object(i)->setTexture((MTL::Texture*)render_pass.color[i].texture->GetHandle());
        colorAttachements->object(i)->setLevel(render_pass.color[i].mip_slice);
        colorAttachements->object(i)->setSlice(render_pass.color[i].array_slice);
        colorAttachements->object(i)->setLoadAction(ToLoadAction(render_pass.color[i].load_op));
        colorAttachements->object(i)->setStoreAction(ToStoreAction(render_pass.color[i].store_op));
        colorAttachements->object(i)->setClearColor(MTL::ClearColor::Make(
            render_pass.color[i].clear_color[0],
            render_pass.color[i].clear_color[1],
            render_pass.color[i].clear_color[2],
            render_pass.color[i].clear_color[3]));
    }
    
    if (render_pass.depth.texture != nullptr)
    {
        MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = descriptor->depthAttachment();
        depthAttachment->setTexture((MTL::Texture*)render_pass.depth.texture->GetHandle());
        depthAttachment->setLevel(render_pass.depth.mip_slice);
        depthAttachment->setSlice(render_pass.depth.array_slice);
        depthAttachment->setLoadAction(ToLoadAction(render_pass.depth.load_op));
        depthAttachment->setStoreAction(ToStoreAction(render_pass.depth.store_op));
        depthAttachment->setClearDepth(render_pass.depth.clear_depth);
        
        if (IsStencilFormat(render_pass.depth.texture->GetDesc().format))
        {
            MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = descriptor->stencilAttachment();
            stencilAttachment->setTexture((MTL::Texture*)render_pass.depth.texture->GetHandle());
            stencilAttachment->setLevel(render_pass.depth.mip_slice);
            stencilAttachment->setSlice(render_pass.depth.array_slice);
            stencilAttachment->setLoadAction(ToLoadAction(render_pass.depth.stencil_load_op));
            stencilAttachment->setStoreAction(ToStoreAction(render_pass.depth.stencil_store_op));
            stencilAttachment->setClearStencil(render_pass.depth.clear_stencil);
        }
    }
    
    RE_ASSERT(m_pRenderCommandEncoder == nullptr);
    m_pRenderCommandEncoder = m_pCommandBuffer->renderCommandEncoder(descriptor);
    
    descriptor->release();
}

void MetalCommandList::EndRenderPass()
{
    m_pRenderCommandEncoder->endEncoding();
    m_pRenderCommandEncoder = nullptr;
}

void MetalCommandList::SetPipelineState(IGfxPipelineState* state)
{
}

void MetalCommandList::SetStencilReference(uint8_t stencil)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
    m_pRenderCommandEncoder->setStencilReferenceValue(stencil);
}

void MetalCommandList::SetBlendFactor(const float* blend_factor)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
    m_pRenderCommandEncoder->setBlendColor(blend_factor[0], blend_factor[1], blend_factor[2], blend_factor[3]);
}

void MetalCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
    
    m_pIndexBuffer = (MTL::Buffer*)buffer->GetHandle();
    m_indexBufferOffset = offset;
    m_indexType = format == GfxFormat::R16UI ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
}

void MetalCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
    
    MTL::Viewport viewport = { x, y, width, height, 0.0, 1.0 };
    m_pRenderCommandEncoder->setViewport(viewport);
}

void MetalCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
    
    MTL::ScissorRect scissorRect = { x, y, width, height };
    m_pRenderCommandEncoder->setScissorRect(scissorRect);
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
