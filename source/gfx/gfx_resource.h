#pragma once

#include "gfx_define.h"

class IGfxDevice;

class IGfxResource
{
public:
    virtual ~IGfxResource() {}

    virtual void* GetHandle() const = 0;

    IGfxDevice* GetDevice() const { return m_pDevice; }
    const eastl::string& GetName() const { return m_name; }

protected:
    IGfxDevice* m_pDevice = nullptr;
    eastl::string m_name;
};