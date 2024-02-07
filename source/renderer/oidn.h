#pragma once

#include "gfx/gfx.h"
#include "EASTL/unique_ptr.h"
#include "OpenImageDenoise/oidn.h"

class Renderer;

class OIDN
{
public:
    OIDN(Renderer* renderer);
    ~OIDN();

    void Reset() { m_bDenoised = false; }
    void Execute(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);

private:
    void InitBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);

private:
    Renderer* m_renderer = nullptr;

    OIDNDevice m_device = nullptr;
    OIDNFilter m_filter = nullptr;
    OIDNBuffer m_colorBuffer = nullptr;
    OIDNBuffer m_albedoBuffer = nullptr;
    OIDNBuffer m_normalBuffer = nullptr;

    eastl::unique_ptr<IGfxTexture> m_colorTexture;
    eastl::unique_ptr<IGfxTexture> m_albedoTexture;
    eastl::unique_ptr<IGfxTexture> m_normalTexture;

    eastl::unique_ptr<IGfxFence> m_fence;
    uint64_t m_fenceValue = 0;
    bool m_bDenoised = false;
};