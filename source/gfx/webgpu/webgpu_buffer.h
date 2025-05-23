#pragma once

#include "../gfx_buffer.h"

class WebGPUDevice;

class WebGPUBuffer : public IGfxBuffer
{
public:
    WebGPUBuffer(WebGPUDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name);
    ~WebGPUBuffer();

    bool Create();

    virtual void* GetHandle() const override;
    virtual void* GetCpuAddress() override;
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual void* GetSharedHandle() const override;

private:
    void* m_pCpuAddress = nullptr;
};
