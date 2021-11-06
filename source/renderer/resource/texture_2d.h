#pragma once

#include "gfx/gfx.h"
#include <memory>

class Renderer;

class Texture2D
{
public:
    Texture2D(const std::string& name);

    bool Create(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV() const { return m_pUAV.get(); }

protected:
    std::string m_name;

    std::unique_ptr<IGfxTexture> m_pTexture;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
    std::unique_ptr<IGfxDescriptor> m_pUAV;
};
