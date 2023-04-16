#pragma once

#include "gfx/gfx.h"
#include "EASTL/unique_ptr.h"

class Renderer;

class Texture2D
{
public:
    Texture2D(const eastl::string& name);

    bool Create(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV(uint32_t mip = 0) const;

protected:
    eastl::string m_name;

    eastl::unique_ptr<IGfxTexture> m_pTexture;
    eastl::unique_ptr<IGfxDescriptor> m_pSRV;
    eastl::vector<eastl::unique_ptr<IGfxDescriptor>> m_UAVs;
};
