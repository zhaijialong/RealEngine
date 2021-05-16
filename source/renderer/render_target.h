#pragma once

#include "gfx/gfx.h"
#include "lsignal/lsignal.h"

#include <memory>

class RenderTarget
{
public:
    RenderTarget(bool auto_resize, float size, const std::string& name);
    ~RenderTarget();

    bool Create(uint32_t width, uint32_t height, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV() const { return m_pUAV.get(); }

private:
    void OnWindowResize(uint32_t width, uint32_t height);

private:
    std::string m_name;
    bool m_bAutoResize;
    float m_size;

    std::unique_ptr<IGfxTexture> m_pTexture;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
    std::unique_ptr<IGfxDescriptor> m_pUAV;


    lsignal::connection m_resizeConnection;
};