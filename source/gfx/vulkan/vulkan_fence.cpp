#include "vulkan_fence.h"
#include "vulkan_device.h"
#include "utils/log.h"

VulkanFence::VulkanFence(VulkanDevice* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

VulkanFence::~VulkanFence()
{
    ((VulkanDevice*)m_pDevice)->Delete(m_semaphore);
}

bool VulkanFence::Create()
{
    VkDevice device = ((VulkanDevice*)m_pDevice)->GetDevice();

    VkSemaphoreTypeCreateInfo timelineCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    createInfo.pNext = &timelineCreateInfo;

    VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &m_semaphore);
    if (result != VK_SUCCESS)
    {
        RE_ERROR("[VulkanFence] failed to create {}", m_name);
        return false;
    }

    SetDebugName(device, VK_OBJECT_TYPE_SEMAPHORE, m_semaphore, m_name.c_str());

    return true;
}

void VulkanFence::Wait(uint64_t value)
{
    VkSemaphoreWaitInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    info.semaphoreCount = 1;
    info.pSemaphores = &m_semaphore;
    info.pValues = &value;

    vkWaitSemaphores((VkDevice)m_pDevice->GetHandle(), &info, UINT64_MAX);
}

void VulkanFence::Signal(uint64_t value)
{
    VkSemaphoreSignalInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    info.semaphore = m_semaphore;
    info.value = value;

    vkSignalSemaphore((VkDevice)m_pDevice->GetHandle(), &info);
}
