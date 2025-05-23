#include "webgpu_fence.h"
#include "webgpu_device.h"

WebGPUFence::WebGPUFence(WebGPUDevice* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

WebGPUFence::~WebGPUFence()
{
}

bool WebGPUFence::Create()
{
    return true;
}

void* WebGPUFence::GetHandle() const
{
    return nullptr;
}

void WebGPUFence::Wait(uint64_t value)
{
}

void WebGPUFence::Signal(uint64_t value)
{
}
