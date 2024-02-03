#pragma once

#include "resource/texture_2d_array.h"

class STBN
{
public:
    STBN(Renderer* renderer);
    void Load(const eastl::string& path);

    IGfxDescriptor* GetScalarTextureSRV() const { return m_scalarTexture->GetSRV(); }
    IGfxDescriptor* GetVec3TextureSRV() const { return m_vec3Texture->GetSRV(); }

private:
    Renderer* m_renderer = nullptr;
    eastl::unique_ptr<Texture2DArray> m_scalarTexture;
    eastl::unique_ptr<Texture2DArray> m_vec3Texture;
};