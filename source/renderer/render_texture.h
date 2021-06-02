#pragma once

#include "gfx/gfx.h"
#include "lsignal/lsignal.h"

#include <memory>

class RenderTexture
{
public:
    RenderTexture(const std::string& name);
    ~RenderTexture();

    //fixed size
    bool Create(uint32_t width, uint32_t height, GfxFormat format, GfxTextureUsageFlags flags);
    //auto-resized with window
    bool Create(void* window_handle, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }
    IGfxDescriptor* GetUAV() const { return m_pUAV.get(); }

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

private:
    std::string m_name;

    std::unique_ptr<IGfxTexture> m_pTexture;
    std::unique_ptr<IGfxDescriptor> m_pSRV;
    std::unique_ptr<IGfxDescriptor> m_pUAV;

    void* m_pWindow = nullptr;
    float m_widthRatio = 0.0f;
    float m_heightRatio = 0.0f;
    lsignal::connection m_resizeConnection;
};