#pragma once

#include "NRD.h"
#include "../resource/texture_2d.h"

struct NRDPoolTexture
{
    eastl::unique_ptr<Texture2D> texture;
    eastl::vector<GfxAccessFlags> states;

    void SetResourceSate(IGfxCommandList* pCommandList, uint32_t mip, GfxAccessFlags state)
    {
        if (states[mip] != state)
        {
            pCommandList->TextureBarrier(texture->GetTexture(), mip, states[mip], state);
            states[mip] = state;
        }
    }
};

struct NRDUserTexture //always 1 mip
{
    IGfxTexture* texture;
    IGfxDescriptor* srv;
    IGfxDescriptor* uav;
    GfxAccessFlags state;

    void SetResourceSate(IGfxCommandList* pCommandList, GfxAccessFlags new_state)
    {
        if (state != state)
        {
            pCommandList->TextureBarrier(texture, 0, state, new_state);
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

    bool Initialize(const nrd::InstanceCreationDesc& desc);
    void Destroy();

    bool SetCommonSettings(const nrd::CommonSettings& commonSettings);
    bool SetDenoiserSettings(nrd::Identifier denoiser, const void* denoiserSettings);

    void Denoise(IGfxCommandList* pCommandList, nrd::Identifier denoiser, NRDUserPool& userPool);

private:
    void CreatePipelines();
    void CreateTextures();
    void CreateSamplers();

    void Dispatch(IGfxCommandList* pCommandList, const nrd::DispatchDesc& dispatchDesc, NRDUserPool& userPool);

private:
    Renderer* m_pRenderer = nullptr;
    nrd::Instance* m_pInstance = nullptr;
    eastl::vector<eastl::unique_ptr<IGfxShader>> m_shaders;
    eastl::vector<eastl::unique_ptr<IGfxPipelineState>> m_pipelines;
    eastl::vector<NRDPoolTexture> m_textures;
    eastl::vector<eastl::unique_ptr<IGfxDescriptor>> m_samplers;
};