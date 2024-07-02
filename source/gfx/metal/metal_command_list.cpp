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
    EndBlitEncoder();
    EndComputeEncoder();
    EndRenderPass();
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
    m_pBlitCommandEncoder = nullptr;
    m_pRenderCommandEncoder = nullptr;
    m_pComputeCommandEncoder = nullptr;
    
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
    BeginBlitEncoder();
    
    const GfxTextureDesc& desc = dst_texture->GetDesc();
    MTL::Size textureSize = MTL::Size::Make(
        eastl::max(desc.width >> mip_level, 1u),
        eastl::max(desc.height >> mip_level, 1u),
        eastl::max(desc.depth >> mip_level, 1u));
    
    uint32_t bytesPerRow = ((MetalTexture*)dst_texture)->GetRowPitch(mip_level);
    
    uint32_t block_height = GetFormatBlockHeight(desc.format);
    uint32_t height = eastl::max(desc.height >> mip_level, block_height);
    uint32_t row_num = height / block_height;
    uint32_t bytesPerImage = bytesPerRow * row_num;
    
    m_pBlitCommandEncoder->copyFromBuffer(
        (MTL::Buffer*)src_buffer->GetHandle(),
        offset,
        bytesPerRow,
        desc.type == GfxTextureType::Texture3D ? bytesPerImage : 0,
        textureSize,
        (MTL::Texture*)dst_texture->GetHandle(),
        array_slice,
        mip_level,
        MTL::Origin::Make(0, 0, 0));
}

void MetalCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
    BeginBlitEncoder();
    
    const GfxTextureDesc& desc = src_texture->GetDesc();
    MTL::Size textureSize = MTL::Size::Make(
        eastl::max(desc.width >> mip_level, 1u),
        eastl::max(desc.height >> mip_level, 1u),
        eastl::max(desc.depth >> mip_level, 1u));
    
    uint32_t bytesPerRow = ((MetalTexture*)src_texture)->GetRowPitch(mip_level);
    
    uint32_t block_height = GetFormatBlockHeight(desc.format);
    uint32_t height = eastl::max(desc.height >> mip_level, block_height);
    uint32_t row_num = height / block_height;
    uint32_t bytesPerImage = bytesPerRow * row_num;
    
    m_pBlitCommandEncoder->copyFromTexture(
        (MTL::Texture*)src_texture->GetHandle(),
        array_slice,
        mip_level,
        MTL::Origin(0, 0, 0),
        textureSize,
        (MTL::Buffer*)dst_buffer->GetHandle(),
        offset,
        bytesPerRow,
        desc.type == GfxTextureType::Texture3D ? bytesPerImage : 0);
}

void MetalCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
    BeginBlitEncoder();
    
    m_pBlitCommandEncoder->copyFromBuffer((MTL::Buffer*)src->GetHandle(), src_offset, (MTL::Buffer*)dst->GetHandle(), dst_offset, size);
}

void MetalCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
    BeginBlitEncoder();
    
    const GfxTextureDesc& desc = src->GetDesc();
    MTL::Size src_size = MTL::Size::Make(
        eastl::max(desc.width >> src_mip, 1u),
        eastl::max(desc.height >> src_mip, 1u),
        eastl::max(desc.depth >> src_mip, 1u));
    
    m_pBlitCommandEncoder->copyFromTexture(
        (MTL::Texture*)src->GetHandle(),
        src_array,
        src_mip,
        MTL::Origin(0, 0, 0),
        src_size,
        (MTL::Texture*)dst->GetHandle(),
        dst_array,
        dst_mip,
        MTL::Origin(0, 0, 0));
}

void MetalCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
}

void MetalCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
}

void MetalCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
    BeginBlitEncoder();
    
    BeginEvent("IGfxCommandList::WriteBuffer");
    
    MTL::Buffer* mtlBuffer = (MTL::Buffer*)buffer->GetHandle();
    m_pBlitCommandEncoder->fillBuffer(mtlBuffer, NS::Range::Make(offset + 0, sizeof(uint8_t)), uint8_t(data >> 0));
    m_pBlitCommandEncoder->fillBuffer(mtlBuffer, NS::Range::Make(offset + 1, sizeof(uint8_t)), uint8_t(data >> 8));
    m_pBlitCommandEncoder->fillBuffer(mtlBuffer, NS::Range::Make(offset + 2, sizeof(uint8_t)), uint8_t(data >> 16));
    m_pBlitCommandEncoder->fillBuffer(mtlBuffer, NS::Range::Make(offset + 3, sizeof(uint8_t)), uint8_t(data >> 24));
    
    EndEvent();
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
    EndBlitEncoder();
    EndComputeEncoder();
    
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
    if(m_pRenderCommandEncoder)
    {
        m_pRenderCommandEncoder->endEncoding();
        m_pRenderCommandEncoder = nullptr;
    }
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
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
}

void MetalCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
}

void MetalCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
}

void MetalCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
}

void MetalCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    RE_ASSERT(m_pRenderCommandEncoder != nullptr);
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

void MetalCommandList::BeginBlitEncoder()
{
    EndRenderPass();
    EndComputeEncoder();
    
    if(m_pBlitCommandEncoder == nullptr)
    {
        m_pBlitCommandEncoder = m_pCommandBuffer->blitCommandEncoder();
    }
}

void MetalCommandList::EndBlitEncoder()
{
    if(m_pBlitCommandEncoder)
    {
        m_pBlitCommandEncoder->endEncoding();
        m_pBlitCommandEncoder = nullptr;
    }
}

void MetalCommandList::BeginComputeEncoder()
{
    EndRenderPass();
    EndBlitEncoder();
    
    if(m_pComputeCommandEncoder == nullptr)
    {
        m_pComputeCommandEncoder = m_pCommandBuffer->computeCommandEncoder();
    }
}

void MetalCommandList::EndComputeEncoder()
{
    if(m_pComputeCommandEncoder)
    {
        m_pComputeCommandEncoder->endEncoding();
        m_pComputeCommandEncoder = nullptr;
    }
}
