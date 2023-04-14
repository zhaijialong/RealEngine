#pragma once

#include "NRD.h"
#include "../resource/texture_2d.h"

class NRDIntegration
{
public:
    NRDIntegration(Renderer* pRenderer);

    bool Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc);
    bool SetMethodSettings(nrd::Method method, const void* methodSettings);
    void Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings);
};