#pragma once

#include "gfx/gfx.h"
#include "EASTL/unique_ptr.h"

class TextureCube
{
public:
    TextureCube(const eastl::string& name);

    bool Create(uint32_t width, uint32_t height, uint32_t levels, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    //TextureCube
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    //Texture2DArray
    IGfxDescriptor* GetArraySRV() const { return m_pArraySRV.get(); }
    //RWTexture2DArray
    IGfxDescriptor* GetUAV(uint32_t mip) const { return m_pUAV[mip].get(); }

protected:
    eastl::string m_name;

    eastl::unique_ptr<IGfxTexture> m_pTexture;
    eastl::unique_ptr<IGfxDescriptor> m_pSRV;
    eastl::unique_ptr<IGfxDescriptor> m_pArraySRV;

    eastl::vector<eastl::unique_ptr<IGfxDescriptor>> m_pUAV;
};