#pragma once

#include "NRD.h"
#include "../resource/texture_2d.h"

struct NRDPoolTexture
{
    eastl::unique_ptr<Texture2D> texture;
    eastl::vector<GfxResourceState> states;

    void SetResourceSate(IGfxCommandList* pCommandList, uint32_t mip, GfxResourceState state)
    {
        if (states[mip] != state)
        {
            pCommandList->ResourceBarrier(texture->GetTexture(), mip, states[mip], state);
            states[mip] = state;
        }
    }
};

struct NRDUserTexture //always 1 mip
{
    IGfxTexture* texture;
    IGfxDescriptor* srv;
    IGfxDescriptor* uav;
    GfxResourceState state;

    void SetResourceSate(IGfxCommandList* pCommandList, GfxResourceState new_state)
    {
        if (state != state)
        {
            pCommandList->ResourceBarrier(texture, 0, state, new_state);
            state = new_state;
        }
    }
};

typedef eastl::array<NRDUserTexture, (size_t)nrd::ResourceType::MAX_NUM - 2> NRDUserPool;

class NRDIntegration
{
public:
    NRDIntegration(Renderer* pRenderer);
    ~NRDIntegration();

    bool Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc);
    void Destroy();

    bool SetMethodSettings(nrd::Method method, const void* methodSettings);
    void Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings, NRDUserPool& userPool);

private:
    void CreatePipelines();
    void CreateTextures();
    void CreateSamplers();

    void Dispatch(IGfxCommandList* pCommandList, const nrd::DispatchDesc& dispatchDesc, NRDUserPool& userPool);

private:
    Renderer* m_pRenderer = nullptr;
    nrd::Denoiser* m_pDenoiser = nullptr;
    eastl::vector<eastl::unique_ptr<IGfxShader>> m_shaders;
    eastl::vector<eastl::unique_ptr<IGfxPipelineState>> m_pipelines;
    eastl::vector<NRDPoolTexture> m_textures;
    eastl::vector<eastl::unique_ptr<IGfxDescriptor>> m_samplers;
};