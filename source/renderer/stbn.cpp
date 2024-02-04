#include "stbn.h"
#include "renderer.h"
#include "texture_loader.h"
#include "utils/fmt.h"
#include "utils/memory.h"

STBN::STBN(Renderer* renderer)
{
    m_renderer = renderer;
}

void STBN::Load(const eastl::string& path)
{
    uint8_t* data = nullptr;

    // scalar
    for (uint32_t i = 0; i < 64; ++i)
    {
        TextureLoader loader;
        loader.Load(fmt::format("{}stbn_scalar_2Dx1Dx1D_128x128x64x1_{}.png", path.c_str(), i).c_str(), false);

        if (i == 0)
        {
            data = (uint8_t*)RE_ALLOC(loader.GetDataSize() * 64);
        }

        memcpy(data + loader.GetDataSize() * i, loader.GetData(), loader.GetDataSize());
    }

    m_scalarTexture.reset(m_renderer->CreateTexture2DArray(128, 128, 1, 64, GfxFormat::R8UNORM, 0, "STBN scalar"));
    if (m_scalarTexture)
    {
        m_renderer->UploadTexture(m_scalarTexture->GetTexture(), data);
    }

    // vec3
    for (uint32_t i = 0; i < 64; ++i)
    {
        TextureLoader loader;
        loader.Load(fmt::format("{}stbn_vec3_2Dx1D_128x128x64_{}.png", path.c_str(), i).c_str(), false);

        if (i == 0)
        {
            data = (uint8_t*)RE_REALLOC(data, loader.GetDataSize() * 64);
        }

        memcpy(data + loader.GetDataSize() * i, loader.GetData(), loader.GetDataSize());
    }

    m_vec3Texture.reset(m_renderer->CreateTexture2DArray(128, 128, 1, 64, GfxFormat::RGBA8UNORM, 0, "STBN vec3"));
    if (m_vec3Texture)
    {
        m_renderer->UploadTexture(m_vec3Texture->GetTexture(), data);
    }

    // vec2
    for (uint32_t i = 0; i < 64; ++i)
    {
        TextureLoader loader;
        loader.Load(fmt::format("{}stbn_vec2_2Dx1D_128x128x64_{}.png", path.c_str(), i).c_str(), false);

        uint32_t sliceSize = loader.GetDataSize() / 2;

        if (i == 0)
        {
            data = (uint8_t*)RE_REALLOC(data, sliceSize * 64);
        }

        byte4* srcData = (byte4*)loader.GetData();

        // rgba8 -> rg8
        for (uint32_t j = 0; j < loader.GetWidth() * loader.GetHeight(); ++j)
        {
            data[sliceSize * i + j * 2 + 0] = srcData[j].x;
            data[sliceSize * i + j * 2 + 1] = srcData[j].y;
        }
    }

    m_vec2Texture.reset(m_renderer->CreateTexture2DArray(128, 128, 1, 64, GfxFormat::RG8UNORM, 0, "STBN vec2"));
    if (m_vec2Texture)
    {
        m_renderer->UploadTexture(m_vec2Texture->GetTexture(), data);
    }

    RE_FREE(data);
}
