#include "vulkan_command_list.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_fence.h"
#include "vulkan_texture.h"
#include "vulkan_descriptor.h"
#include "vulkan_descriptor_allocator.h"
#include "vulkan_constant_buffer_allocator.h"
#include "renderer/clear_uav.h"
#include "utils/profiler.h"
#include "../gfx.h"

VulkanCommandList::VulkanCommandList(VulkanDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_queueType = queue_type;
}

VulkanCommandList::~VulkanCommandList()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_commandPool);
}

bool VulkanCommandList::Create()
{
    VulkanDevice* pDevice = (VulkanDevice*)m_pDevice;

    VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };

    switch (m_queueType)
    {
    case GfxCommandQueue::Graphics:
        createInfo.queueFamilyIndex = pDevice->GetGraphicsQueueIndex();
        m_queue = pDevice->GetGraphicsQueue();
        break;
    case GfxCommandQueue::Compute:
        createInfo.queueFamilyIndex = pDevice->GetComputeQueueIndex();
        m_queue = pDevice->GetComputeQueue();
        break;
    case GfxCommandQueue::Copy:
        createInfo.queueFamilyIndex = pDevice->GetCopyQueueIndex();
        m_queue = pDevice->GetCopyQueue();
        break;
    default:
        break;
    }

    VkResult result = vkCreateCommandPool(pDevice->GetDevice(), &createInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    SetDebugName(pDevice->GetDevice(), VK_OBJECT_TYPE_COMMAND_POOL, m_commandPool, m_name.c_str());

    return true;
}

void VulkanCommandList::ResetAllocator()
{
    vkResetCommandPool((VkDevice)m_pDevice->GetHandle(), m_commandPool, 0);

    for (size_t i = 0; i < m_pendingCommandBuffers.size(); ++i)
    {
        m_freeCommandBuffers.push_back(m_pendingCommandBuffers[i]);
    }
    m_pendingCommandBuffers.clear();
}

void VulkanCommandList::Begin()
{
    if (!m_freeCommandBuffers.empty())
    {
        m_commandBuffer = m_freeCommandBuffers.back();
        m_freeCommandBuffers.pop_back();
    }
    else
    {
        VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        info.commandPool = m_commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;

        vkAllocateCommandBuffers((VkDevice)m_pDevice->GetHandle(), &info, &m_commandBuffer);
    }

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    ResetState();
}

void VulkanCommandList::End()
{
    FlushBarriers();

    vkEndCommandBuffer(m_commandBuffer);

    m_pendingCommandBuffers.push_back(m_commandBuffer);
}

void VulkanCommandList::Wait(IGfxFence* fence, uint64_t value)
{
    m_pendingWaits.emplace_back(fence, value);
}

void VulkanCommandList::Signal(IGfxFence* fence, uint64_t value)
{
    m_pendingSignals.emplace_back(fence, value);
}

void VulkanCommandList::Present(IGfxSwapchain* swapchain)
{
    m_pendingSwapchain.push_back(swapchain);
}

void VulkanCommandList::Submit()
{
    ((VulkanDevice*)m_pDevice)->FlushLayoutTransition(m_queueType);

    eastl::vector<VkSemaphore> waitSemaphores;
    eastl::vector<VkSemaphore> signalSemaphores;
    eastl::vector<uint64_t> waitValues;
    eastl::vector<uint64_t> signalValues;
    eastl::vector<VkPipelineStageFlags> waitStages;

    for (size_t i = 0; i < m_pendingWaits.size(); ++i)
    {
        waitSemaphores.push_back((VkSemaphore)m_pendingWaits[i].first->GetHandle());
        waitStages.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        waitValues.push_back(m_pendingWaits[i].second);
    }
    m_pendingWaits.clear();

    for (size_t i = 0; i < m_pendingSignals.size(); ++i)
    {
        signalSemaphores.push_back((VkSemaphore)m_pendingSignals[i].first->GetHandle());
        signalValues.push_back(m_pendingSignals[i].second);
    }
    m_pendingSignals.clear();

    for (size_t i = 0; i < m_pendingSwapchain.size(); ++i)
    {
        VulkanSwapchain* swapchain = (VulkanSwapchain*)m_pendingSwapchain[i];
        
        waitSemaphores.push_back(swapchain->GetAcquireSemaphore());
        waitStages.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        signalSemaphores.push_back(swapchain->GetPresentSemaphore());

        // these are binary semaphores, wait/signal values are just ignored
        waitValues.push_back(0);
        signalValues.push_back(0);
    }

    VkTimelineSemaphoreSubmitInfo timelineInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    timelineInfo.waitSemaphoreValueCount = (uint32_t)waitValues.size();
    timelineInfo.pWaitSemaphoreValues = waitValues.data();
    timelineInfo.signalSemaphoreValueCount = (uint32_t)signalValues.size();
    timelineInfo.pSignalSemaphoreValues = signalValues.data();

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.pNext = &timelineInfo;
    submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

    for (size_t i = 0; i < m_pendingSwapchain.size(); ++i)
    {
        ((VulkanSwapchain*)m_pendingSwapchain[i])->Present(m_queue);
    }
    m_pendingSwapchain.clear();
}

void VulkanCommandList::ResetState()
{
    if (m_queueType == GfxCommandQueue::Graphics || m_queueType == GfxCommandQueue::Compute)
    {
        VulkanDevice* device = (VulkanDevice*)m_pDevice;

        VkDescriptorBufferBindingInfoEXT descriptorBuffer[3] = {};
        descriptorBuffer[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        descriptorBuffer[0].address = device->GetConstantBufferAllocator()->GetGpuAddress();
        descriptorBuffer[0].usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
        descriptorBuffer[1].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        descriptorBuffer[1].address = device->GetResourceDescriptorAllocator()->GetGpuAddress();
        descriptorBuffer[1].usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
        descriptorBuffer[2].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        descriptorBuffer[2].address = device->GetSamplerDescriptorAllocator()->GetGpuAddress();
        descriptorBuffer[2].usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

        vkCmdBindDescriptorBuffersEXT(m_commandBuffer, 3, descriptorBuffer);

        uint32_t bufferIndices[] = { 1, 2 };
        VkDeviceSize offsets[] = { 0, 0 };

        vkCmdSetDescriptorBufferOffsetsEXT(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, device->GetPipelineLayout(), 1, 2, bufferIndices, offsets);

        if (m_queueType == GfxCommandQueue::Graphics)
        {
            vkCmdSetDescriptorBufferOffsetsEXT(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device->GetPipelineLayout(), 1, 2, bufferIndices, offsets);
        }
    }
}

void VulkanCommandList::BeginProfiling()
{
}

void VulkanCommandList::EndProfiling()
{
}

void VulkanCommandList::BeginEvent(const eastl::string& event_name)
{
    VkDebugUtilsLabelEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    info.pLabelName = event_name.c_str();

    vkCmdBeginDebugUtilsLabelEXT(m_commandBuffer, &info);
}

void VulkanCommandList::EndEvent()
{
    vkCmdEndDebugUtilsLabelEXT(m_commandBuffer);
}

void VulkanCommandList::CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset)
{
    FlushBarriers();

    const GfxTextureDesc& desc = dst_texture->GetDesc();

    VkBufferImageCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    copy.bufferOffset = offset;
    copy.imageSubresource.aspectMask = GetAspectFlags(desc.format);
    copy.imageSubresource.mipLevel = mip_level;
    copy.imageSubresource.baseArrayLayer = array_slice;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = eastl::max(desc.width >> mip_level, 1u);
    copy.imageExtent.height = eastl::max(desc.height >> mip_level, 1u);
    copy.imageExtent.depth = eastl::max(desc.depth >> mip_level, 1u);

    VkCopyBufferToImageInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    info.srcBuffer = (VkBuffer)src_buffer->GetHandle();
    info.dstImage = (VkImage)dst_texture->GetHandle();
    info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    info.regionCount = 1;
    info.pRegions = &copy;

    vkCmdCopyBufferToImage2(m_commandBuffer, &info);
}

void VulkanCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
    FlushBarriers();

    const GfxTextureDesc& desc = src_texture->GetDesc();

    VkBufferImageCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    copy.bufferOffset = offset;
    copy.imageSubresource.aspectMask = GetAspectFlags(desc.format);
    copy.imageSubresource.mipLevel = mip_level;
    copy.imageSubresource.baseArrayLayer = array_slice;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = eastl::max(desc.width >> mip_level, 1u);
    copy.imageExtent.height = eastl::max(desc.height >> mip_level, 1u);
    copy.imageExtent.depth = eastl::max(desc.depth >> mip_level, 1u);

    VkCopyImageToBufferInfo2 info = { VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2 };
    info.srcImage = (VkImage)src_texture->GetHandle();
    info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    info.dstBuffer = (VkBuffer)dst_buffer->GetHandle();
    info.regionCount = 1;
    info.pRegions = &copy;

    vkCmdCopyImageToBuffer2(m_commandBuffer, &info);
}

void VulkanCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
    FlushBarriers();

    VkBufferCopy2 copy = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    copy.srcOffset = src_offset;
    copy.dstOffset = dst_offset;
    copy.size = size;

    VkCopyBufferInfo2 info = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    info.srcBuffer = (VkBuffer)src->GetHandle();
    info.dstBuffer = (VkBuffer)dst->GetHandle();
    info.regionCount = 1;
    info.pRegions = &copy;

    vkCmdCopyBuffer2(m_commandBuffer, &info);
}

void VulkanCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
    FlushBarriers();

    VkImageCopy2 copy = { VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
    copy.srcSubresource.aspectMask = GetAspectFlags(src->GetDesc().format);
    copy.srcSubresource.mipLevel = src_mip;
    copy.srcSubresource.baseArrayLayer = src_array;
    copy.srcSubresource.layerCount = 1;
    copy.dstSubresource.aspectMask = GetAspectFlags(dst->GetDesc().format);
    copy.dstSubresource.mipLevel = dst_mip;
    copy.dstSubresource.baseArrayLayer = dst_array;
    copy.dstSubresource.layerCount = 1;
    copy.extent.width = eastl::max(src->GetDesc().width >> src_mip, 1u);
    copy.extent.height = eastl::max(src->GetDesc().height >> src_mip, 1u);
    copy.extent.depth = eastl::max(src->GetDesc().depth >> src_mip, 1u);

    VkCopyImageInfo2 info = { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
    info.srcImage = (VkImage)src->GetHandle();
    info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    info.dstImage = (VkImage)dst->GetHandle();
    info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    info.regionCount = 1;
    info.pRegions = &copy;

    vkCmdCopyImage2(m_commandBuffer, &info);
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
    const GfxUnorderedAccessViewDesc& desc = ((VulkanUnorderedAccessView*)uav)->GetDesc();
    ::ClearUAV(this, resource, uav, desc, clear_value);
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
    const GfxUnorderedAccessViewDesc& desc = ((VulkanUnorderedAccessView*)uav)->GetDesc();
    ::ClearUAV(this, resource, uav, desc, clear_value);
}

void VulkanCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
    FlushBarriers();

    vkCmdUpdateBuffer(m_commandBuffer, (VkBuffer)buffer->GetHandle(), offset, sizeof(uint32_t), &data);
}

void VulkanCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
    //todo
}

void VulkanCommandList::TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.image = (VkImage)texture->GetHandle();
    barrier.srcStageMask = GetStageMask(access_before);
    barrier.dstStageMask = GetStageMask(access_after);
    barrier.srcAccessMask = GetAccessMask(access_before);
    barrier.dstAccessMask = GetAccessMask(access_after);
    barrier.oldLayout = GetImageLayout(access_before);
    if (access_after & GfxAccessDiscard)
    {
        barrier.newLayout = barrier.oldLayout; // new layout can't be undefined
    }
    else
    {
        barrier.newLayout = GetImageLayout(access_after);
    }
    barrier.subresourceRange.aspectMask = GetAspectFlags(texture->GetDesc().format);

    if (sub_resource == GFX_ALL_SUB_RESOURCE)
    {
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }
    else
    {
        uint32_t mip, slice;
        DecomposeSubresource(texture->GetDesc(), sub_resource, mip, slice);

        barrier.subresourceRange.baseMipLevel = mip;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = slice;
        barrier.subresourceRange.layerCount = 1;
    }

    m_imageBarriers.push_back(barrier);
}

void VulkanCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
    VkBufferMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    barrier.buffer = (VkBuffer)buffer->GetHandle();
    barrier.offset = 0;
    barrier.size = buffer->GetDesc().size;
    barrier.srcStageMask = GetStageMask(access_before);
    barrier.dstStageMask = GetStageMask(access_after);
    barrier.srcAccessMask = GetAccessMask(access_before);
    barrier.dstAccessMask = GetAccessMask(access_after);

    m_bufferBarriers.push_back(barrier);
}

void VulkanCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = GetStageMask(access_before);
    barrier.dstStageMask = GetStageMask(access_after);
    barrier.srcAccessMask = GetAccessMask(access_before);
    barrier.dstAccessMask = GetAccessMask(access_after);

    m_memoryBarriers.push_back(barrier);
}

void VulkanCommandList::FlushBarriers()
{
    if (!m_memoryBarriers.empty() || !m_bufferBarriers.empty() || !m_imageBarriers.empty())
    {
        VkDependencyInfo info = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = (uint32_t)m_memoryBarriers.size();
        info.pMemoryBarriers = m_memoryBarriers.data();
        info.bufferMemoryBarrierCount = (uint32_t)m_bufferBarriers.size();
        info.pBufferMemoryBarriers = m_bufferBarriers.data();
        info.imageMemoryBarrierCount = (uint32_t)m_imageBarriers.size();
        info.pImageMemoryBarriers = m_imageBarriers.data();

        vkCmdPipelineBarrier2(m_commandBuffer, &info);

        m_memoryBarriers.clear();
        m_bufferBarriers.clear();
        m_imageBarriers.clear();
    }
}

void VulkanCommandList::BeginRenderPass(const GfxRenderPassDesc& render_pass)
{
    FlushBarriers();

    VkRenderingAttachmentInfo colorAttachments[8] = {};
    VkRenderingAttachmentInfo depthAttachments = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    VkRenderingAttachmentInfo stencilAttachments = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    uint32_t width = 0;
    uint32_t height = 0;

    for (uint32_t i = 0; i < 8; ++i)
    {
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        if (render_pass.color[i].texture)
        {
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

            colorAttachments[i].imageView = ((VulkanTexture*)render_pass.color[i].texture)->GetRenderView(render_pass.color[i].mip_slice, render_pass.color[i].array_slice);
            colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments[i].loadOp = GetLoadOp(render_pass.color[i].load_op);
            colorAttachments[i].storeOp = GetStoreOp(render_pass.color[i].store_op);
            memcpy(colorAttachments[i].clearValue.color.float32, render_pass.color[i].clear_color, sizeof(float) * 4);
        }
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

        depthAttachments.imageView = ((VulkanTexture*)render_pass.depth.texture)->GetRenderView(render_pass.depth.mip_slice, render_pass.depth.array_slice);
        depthAttachments.imageLayout = render_pass.depth.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachments.loadOp = GetLoadOp(render_pass.depth.load_op);
        depthAttachments.storeOp = GetStoreOp(render_pass.depth.store_op);

        if (IsStencilFormat(render_pass.depth.texture->GetDesc().format))
        {
            stencilAttachments.imageView = ((VulkanTexture*)render_pass.depth.texture)->GetRenderView(render_pass.depth.mip_slice, render_pass.depth.array_slice);
            stencilAttachments.imageLayout = render_pass.depth.read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            stencilAttachments.loadOp = GetLoadOp(render_pass.depth.stencil_load_op);
            stencilAttachments.storeOp = GetStoreOp(render_pass.depth.stencil_store_op);
        }
    }

    VkRenderingInfo info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    info.renderArea.extent.width = width;
    info.renderArea.extent.height = height;
    info.layerCount = 1;
    info.viewMask = 0;
    info.colorAttachmentCount = 8;
    info.pColorAttachments = colorAttachments;

    if (depthAttachments.imageView != VK_NULL_HANDLE)
    {
        info.pDepthAttachment = &depthAttachments;
    }

    if (stencilAttachments.imageView != VK_NULL_HANDLE)
    {
        info.pStencilAttachment = &stencilAttachments;
    }

    vkCmdBeginRendering(m_commandBuffer, &info);

    SetViewport(0, 0, width, height);
}

void VulkanCommandList::EndRenderPass()
{
    vkCmdEndRendering(m_commandBuffer);
}

void VulkanCommandList::SetPipelineState(IGfxPipelineState* state)
{
    VkPipelineBindPoint bindPoint = state->GetType() == GfxPipelineType::Compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(m_commandBuffer, bindPoint, (VkPipeline)state->GetHandle());
}

void VulkanCommandList::SetStencilReference(uint8_t stencil)
{
    vkCmdSetStencilReference(m_commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencil);
}

void VulkanCommandList::SetBlendFactor(const float* blend_factor)
{
    vkCmdSetBlendConstants(m_commandBuffer, blend_factor);
}

void VulkanCommandList::SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
{
    VkIndexType type = format == GfxFormat::R16UI ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(m_commandBuffer, (VkBuffer)buffer->GetHandle(), (VkDeviceSize)offset, type);
}

void VulkanCommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    // negate the height and move the origin to the bottom left, to match d3d12's ndc space

    VkViewport viewport;
    viewport.x = x;
    viewport.y = (float)height - (float)y;
    viewport.width = width;
    viewport.height = -(float)height;
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

    SetScissorRect(x, y, width, height);
}

void VulkanCommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    VkRect2D scissor;
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;

    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
}

void VulkanCommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size)
{
    if (slot == 0)
    {
        RE_ASSERT(data_size <= GFX_MAX_ROOT_CONSTANTS * sizeof(uint32_t));
        memcpy(m_graphicsConstants.cbv0, data, data_size);
    }
    else
    {
        RE_ASSERT(slot < GFX_MAX_CBV_BINDINGS);
        VkDeviceAddress gpuAddress = ((VulkanDevice*)m_pDevice)->AllocateConstantBuffer(data, data_size);

        if (slot == 1)
        {
            m_graphicsConstants.cbv1.address = gpuAddress;
            m_graphicsConstants.cbv1.range = data_size;
        }
        else
        {
            m_graphicsConstants.cbv2.address = gpuAddress;
            m_graphicsConstants.cbv2.range = data_size;
        }
    }

    m_graphicsConstants.dirty = true;
}

void VulkanCommandList::SetComputeConstants(uint32_t slot, const void* data, size_t data_size)
{
    if (slot == 0)
    {
        RE_ASSERT(data_size <= GFX_MAX_ROOT_CONSTANTS * sizeof(uint32_t));
        memcpy(m_computeConstants.cbv0, data, data_size);
    }
    else
    {
        RE_ASSERT(slot < GFX_MAX_CBV_BINDINGS);
        VkDeviceAddress gpuAddress = ((VulkanDevice*)m_pDevice)->AllocateConstantBuffer(data, data_size);

        if (slot == 1)
        {
            m_computeConstants.cbv1.address = gpuAddress;
            m_computeConstants.cbv1.range = data_size;
        }
        else
        {
            m_computeConstants.cbv2.address = gpuAddress;
            m_computeConstants.cbv2.range = data_size;
        }
    }

    m_computeConstants.dirty = true;
}

void VulkanCommandList::Draw(uint32_t vertex_count, uint32_t instance_count)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDraw(m_commandBuffer, vertex_count, instance_count, 0, 0);
}

void VulkanCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t index_offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawIndexed(m_commandBuffer, index_count, instance_count, index_offset, 0, 0);
}

void VulkanCommandList::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    FlushBarriers();
    UpdateComputeDescriptorBuffer();

    vkCmdDispatch(m_commandBuffer, group_count_x, group_count_y, group_count_z);
}

void VulkanCommandList::DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawMeshTasksEXT(m_commandBuffer, group_count_x, group_count_y, group_count_z);
}

void VulkanCommandList::DrawIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawIndirect(m_commandBuffer, (VkBuffer)buffer->GetHandle(), offset, 1, 0);
}

void VulkanCommandList::DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawIndexedIndirect(m_commandBuffer, (VkBuffer)buffer->GetHandle(), offset, 1, 0);
}

void VulkanCommandList::DispatchIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    FlushBarriers();
    UpdateComputeDescriptorBuffer();

    vkCmdDispatchIndirect(m_commandBuffer, (VkBuffer)buffer->GetHandle(), offset);
}

void VulkanCommandList::DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawMeshTasksIndirectEXT(m_commandBuffer, (VkBuffer)buffer->GetHandle(), offset, 1, 0);
}

void VulkanCommandList::MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawIndirectCount(m_commandBuffer, (VkBuffer)args_buffer->GetHandle(), args_buffer_offset,
        (VkBuffer)count_buffer->GetHandle(), count_buffer_offset, max_count, sizeof(GfxDrawCommand));
}

void VulkanCommandList::MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawIndexedIndirectCount(m_commandBuffer, (VkBuffer)args_buffer->GetHandle(), args_buffer_offset,
        (VkBuffer)count_buffer->GetHandle(), count_buffer_offset, max_count, sizeof(GfxDrawIndexedCommand));
}

void VulkanCommandList::MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    FlushBarriers();
    UpdateComputeDescriptorBuffer();

    // not supported
    RE_ASSERT(false);
}

void VulkanCommandList::MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset)
{
    UpdateGraphicsDescriptorBuffer();

    vkCmdDrawMeshTasksIndirectCountEXT(m_commandBuffer, (VkBuffer)args_buffer->GetHandle(), args_buffer_offset,
        (VkBuffer)count_buffer->GetHandle(), count_buffer_offset, max_count, sizeof(GfxDispatchCommand));
}

void VulkanCommandList::BuildRayTracingBLAS(IGfxRayTracingBLAS* blas)
{
    FlushBarriers();
    
    //todo
}

void VulkanCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    FlushBarriers();
    
    //todo
}

void VulkanCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    FlushBarriers();
    
    //todo
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* VulkanCommandList::GetProfileLog() const
{
    return nullptr;
}
#endif

void VulkanCommandList::UpdateGraphicsDescriptorBuffer()
{
    if (m_graphicsConstants.dirty)
    {
        VulkanDevice* device = (VulkanDevice*)m_pDevice;
        VkDeviceSize cbvDescriptorOffset = device->AllocateConstantBufferDescriptor(m_graphicsConstants.cbv0, m_graphicsConstants.cbv1, m_graphicsConstants.cbv2);

        uint32_t bufferIndices[] = { 0 };
        VkDeviceSize offsets[] = { cbvDescriptorOffset };

        vkCmdSetDescriptorBufferOffsetsEXT(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device->GetPipelineLayout(), 0, 1, bufferIndices, offsets);

        m_graphicsConstants.dirty = false;
    }
}

void VulkanCommandList::UpdateComputeDescriptorBuffer()
{
    if (m_computeConstants.dirty)
    {
        VulkanDevice* device = (VulkanDevice*)m_pDevice;
        VkDeviceSize cbvDescriptorOffset = device->AllocateConstantBufferDescriptor(m_computeConstants.cbv0, m_computeConstants.cbv1, m_computeConstants.cbv2);

        uint32_t bufferIndices[] = { 0 };
        VkDeviceSize offsets[] = { cbvDescriptorOffset };

        vkCmdSetDescriptorBufferOffsetsEXT(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, device->GetPipelineLayout(), 0, 1, bufferIndices, offsets);

        m_computeConstants.dirty = false;
    }
}
