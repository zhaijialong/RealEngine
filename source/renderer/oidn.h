#pragma once

#if WITH_OIDN

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
    void ReleaseBuffers();

    void InitCPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);
    void InitGPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);
    void ExecuteCPU(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);
    void ExecuteGPU(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal);

private:
    Renderer* m_renderer = nullptr;

    OIDNDevice m_device = nullptr;
    OIDNFilter m_filter = nullptr;
    OIDNBuffer m_oidnColorBuffer = nullptr;
    OIDNBuffer m_oidnAlbedoBuffer = nullptr;
    OIDNBuffer m_oidnNormalBuffer = nullptr;

    // readback buffers for the cpu path, or shared buffers for the gpu path
    eastl::unique_ptr<IGfxBuffer> m_colorBuffer;
    eastl::unique_ptr<IGfxBuffer> m_albedoBuffer;
    eastl::unique_ptr<IGfxBuffer> m_normalBuffer;

    eastl::unique_ptr<IGfxBuffer> m_colorUploadBuffer;

    eastl::unique_ptr<IGfxFence> m_fence;
    uint64_t m_fenceValue = 0;
    bool m_bDenoised = false;
};

#endif // WITH_OIDN