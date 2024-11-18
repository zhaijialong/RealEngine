#pragma once

#include "gfx_resource.h"

class IGfxBuffer : public IGfxResource
{
public:
    const GfxBufferDesc& GetDesc() const { return m_desc; }

    virtual bool IsBuffer() const { return true; }
    virtual void* GetCpuAddress() = 0;
    virtual uint64_t GetGpuAddress() = 0;
    virtual uint32_t GetRequiredStagingBufferSize() const = 0;
    virtual void* GetSharedHandle() const = 0;

protected:
    GfxBufferDesc m_desc = {};
};
