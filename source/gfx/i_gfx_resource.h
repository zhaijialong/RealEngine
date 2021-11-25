#pragma once

#include "gfx_define.h"
#include <string>

class IGfxDevice;

class IGfxResource
{
public:
    virtual ~IGfxResource() {}

    virtual void* GetHandle() const = 0;

    IGfxDevice* GetDevice() const { return m_pDevice; }
    const std::string& GetName() const { return m_name; }

protected:
    IGfxDevice* m_pDevice = nullptr;
    std::string m_name;
};