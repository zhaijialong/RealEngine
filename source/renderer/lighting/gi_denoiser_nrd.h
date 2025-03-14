#pragma once

#include "core/platform.h"

#if RE_PLATFORM_WINDOWS

#include "../render_graph.h"
#include "../resource/texture_2d.h"

class NRDIntegration;
namespace nrd { struct ReblurSettings; }

class GIDenoiserNRD
{
public:
    GIDenoiserNRD(Renderer* pRenderer);
    ~GIDenoiserNRD();

    void ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height);
    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle radiance, RGHandle rayDirection, RGHandle normal, RGHandle linearDepth, RGHandle velocity, uint32_t width, uint32_t height);

    RGHandle GetHistoryIrradiance() const { return m_historyIrradiance; }
    IGfxDescriptor* GetHistoryIrradianceSRV() const { return m_pResolvedOutput->GetSRV(); }

private:
    void CreateReblurDenoiser(uint32_t width, uint32_t height);

    void PackNormalRoughness(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* packedNormal, uint32_t width, uint32_t height);
    void PackRadiance(IGfxCommandList* pCommandList, RGTexture* radiance, RGTexture* rayDirection, RGTexture* linearDepth, RGTexture* outputSH0, RGTexture* outputSH1, uint32_t width, uint32_t height);
    void PackVelocity(IGfxCommandList* pCommandList, RGTexture* velocity, RGTexture* output, uint32_t width, uint32_t height);
    void ResolveOutput(IGfxCommandList* pCommandList, RGTexture* normal, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer = nullptr;
    eastl::unique_ptr<NRDIntegration> m_pReblur;
    eastl::unique_ptr<nrd::ReblurSettings> m_pReblurSettings;
    bool m_bNeedCreateReblur = true;
    bool m_bHistoryInvalid = true;

    eastl::unique_ptr<Texture2D> m_pOutputSH0;
    eastl::unique_ptr<Texture2D> m_pOutputSH1;
    eastl::unique_ptr<Texture2D> m_pResolvedOutput;
    RGHandle m_historyIrradiance;

    IGfxPipelineState* m_pPackNormalRoughnessPSO = nullptr;
    IGfxPipelineState* m_pPackRadianceHitTPSO = nullptr;
    IGfxPipelineState* m_pPackVelocityPSO = nullptr;
    IGfxPipelineState* m_pResolveOutputPSO = nullptr;
};

#endif // #if RE_PLATFORM_WINDOWS
