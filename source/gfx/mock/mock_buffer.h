#pragma once

#include "../gfx_buffer.h"

class MockDevice;

class MockBuffer : public IGfxBuffer
{
public:
    MockBuffer(MockDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name);
    ~MockBuffer();

    bool Create();

    virtual void* GetHandle() const override;
    virtual void* GetCpuAddress() override;
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;

private:
    void* m_pCpuAddress = nullptr;
};
