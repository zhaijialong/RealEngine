#pragma once

#include "metal_utils.h"
#include "../gfx_buffer.h"

class MetalDevice;

class MetalBuffer : public IGfxBuffer
{
public:
    MetalBuffer(MetalDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name);
    ~MetalBuffer();

    bool Create();

    virtual void* GetHandle() const override { return m_pBuffer; }
    virtual void* GetCpuAddress() override { return m_pCpuAddress; }
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;

private:
    MTL::Buffer* m_pBuffer = nullptr;
    void* m_pCpuAddress = nullptr;
};
