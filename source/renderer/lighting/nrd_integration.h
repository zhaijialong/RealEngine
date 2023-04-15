#pragma once

#include "NRD.h"
#include "../resource/texture_2d.h"

struct NRDIntegrationTexture
{
    eastl::unique_ptr<Texture2D> texture;
    eastl::vector<GfxResourceState> states;
};

class NRDIntegration
{
public:
    NRDIntegration(Renderer* pRenderer);
    ~NRDIntegration();

    bool Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc);
    void Destroy();

    bool SetMethodSettings(nrd::Method method, const void* methodSettings);
    void Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings);

private:
    void CreatePipelines();
    void CreateTextures();
    void CreateSamplers();

    void Dispatch(IGfxCommandList* pCommandList, const nrd::DispatchDesc& dispatchDesc);

private:
    Renderer* m_pRenderer = nullptr;
    nrd::Denoiser* m_pDenoiser = nullptr;
    eastl::vector<eastl::unique_ptr<IGfxShader>> m_shaders;
    eastl::vector<eastl::unique_ptr<IGfxPipelineState>> m_pipelines;
    eastl::vector<NRDIntegrationTexture> m_textures;
    eastl::vector<eastl::unique_ptr<IGfxDescriptor>> m_samplers;
};