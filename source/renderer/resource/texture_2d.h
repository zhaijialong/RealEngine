#pragma once

#include "gfx/gfx.h"
#include "lsignal/lsignal.h"
#include <memory>

class Renderer;

class Texture2D
{
public:
    Texture2D(const std::string& name);
    virtual ~Texture2D() {}

    bool Create(uint32_t width, uint32_t height, GfxFormat format, GfxTextureUsageFlags flags);
    bool Create(void* window_handle, float width_ratio, float height_ratio, GfxFormat format, GfxTextureUsageFlags flags);

    IGfxTexture* GetTexture() const { return m_pTexture.get(); }
    IGfxDescriptor* GetSRV() const { return m_pSRV.get(); }

protected:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

protected:
    std::string m_name;

    std::unique_ptr<IGfxTexture> m_pTexture;
    std::unique_ptr<IGfxDescriptor> m_pSRV;

    void* m_pWindow = nullptr;
    float m_widthRatio = 0.0f;
    float m_heightRatio = 0.0f;
    lsignal::connection m_resizeConnection;
};

class RWTexture2D : public Texture2D
{
private:
    std::unique_ptr<IGfxDescriptor> m_pUAV;
};