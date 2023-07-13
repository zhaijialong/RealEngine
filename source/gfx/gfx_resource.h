#pragma once

#include "gfx_defines.h"

class IGfxDevice;

class IGfxResource
{
public:
    virtual ~IGfxResource() {}

    virtual void* GetHandle() const = 0;
    virtual bool IsTexture() const { return false; }
    virtual bool IsBuffer() const { return false; }

    IGfxDevice* GetDevice() const { return m_pDevice; }
    const eastl::string& GetName() const { return m_name; }

protected:
    IGfxDevice* m_pDevice = nullptr;
    eastl::string m_name;
};