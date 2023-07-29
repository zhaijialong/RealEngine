#pragma once

#include "../gfx_buffer.h"

class MetalBuffer : public IGfxBuffer
{
public:
    virtual void* GetHandle() const override;
    virtual void* GetCpuAddress() override;
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;

};
