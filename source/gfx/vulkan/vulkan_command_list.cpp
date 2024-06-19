#include "vulkan_command_list.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_fence.h"
#include "vulkan_texture.h"
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
    FlushBarriers();
}

void VulkanCommandList::CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice)
{
    FlushBarriers();
}

void VulkanCommandList::CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size)
{
    FlushBarriers();
}

void VulkanCommandList::CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array)
{
    FlushBarriers();
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value)
{
    FlushBarriers();
}

void VulkanCommandList::ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value)
{
    FlushBarriers();
}

void VulkanCommandList::WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data)
{
    FlushBarriers();
}

void VulkanCommandList::UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings)
{
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
    barrier.newLayout = GetImageLayout(access_after);
    barrier.subresourceRange.aspectMask = GetAspectFlags(texture->GetDesc().format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    m_imageBarriers.push_back(barrier);
}

void VulkanCommandList::BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after)
{
}

void VulkanCommandList::GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after)
{
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
    uint32_t colorAttachmentCount = 0;

    for (uint32_t i = 0; i < 8; ++i)
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

        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[i].imageView = ((VulkanTexture*)render_pass.color[i].texture)->GetRenderView(render_pass.color[i].mip_slice, render_pass.color[i].array_slice);
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = GetLoadOp(render_pass.color[i].load_op);
        colorAttachments[i].storeOp = GetStoreOp(render_pass.color[i].store_op);
        memcpy(colorAttachments[i].clearValue.color.float32, render_pass.color[i].clear_color, sizeof(float) * 4);

        ++colorAttachmentCount;
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
    info.colorAttachmentCount = colorAttachmentCount;
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
}

void VulkanCommandList::EndRenderPass()
{
    vkCmdEndRendering(m_commandBuffer);
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
    FlushBarriers();
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
    FlushBarriers();
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
    FlushBarriers();
}

void VulkanCommandList::UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    FlushBarriers();
}

void VulkanCommandList::BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    FlushBarriers();
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* VulkanCommandList::GetProfileLog() const
{
    return nullptr;
}
#endif
