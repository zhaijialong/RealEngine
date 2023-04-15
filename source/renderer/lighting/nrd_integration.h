#pragma once

#include "NRD.h"
#include "../resource/texture_2d.h"

class NRDIntegration
{
public:
    NRDIntegration(Renderer* pRenderer);

    bool Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc);
    void Destroy();

    bool SetMethodSettings(nrd::Method method, const void* methodSettings);
    void Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings);

private:
    void CreatePipelines();
    void CreateResources();

private:
    Renderer* m_pRenderer = nullptr;
    nrd::Denoiser* m_pDenoiser = nullptr;
    eastl::vector<eastl::unique_ptr<IGfxShader>> m_Shaders;
    eastl::vector<eastl::unique_ptr<IGfxPipelineState>> m_pipelines;
};