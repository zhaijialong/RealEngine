#pragma once

#include "gfx/gfx.h"
#include <memory>

class Renderer;

class Texture
{
public:
    Texture(Renderer* pRenderer, const std::string& file);

    bool Create();

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }

private:
    Renderer* m_pRenderer;
    std::string m_file;
    std::unique_ptr<IGfxTexture> m_pTexture;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
};