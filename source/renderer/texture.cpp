#include "texture.h"
#include "renderer.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

Texture::Texture(Renderer* pRenderer, const std::string& file)
{
    m_pRenderer = pRenderer;
    m_file = file;
}

bool Texture::Create()
{
    int x, y, comp;
    stbi_uc* data = stbi_load(m_file.c_str(), &x, &y, &comp, STBI_rgb_alpha);
    if (data == nullptr)
    {
        return false;
    }

    IGfxDevice* pDevice = m_pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = x;
    desc.height = y;
    desc.format = GfxFormat::RGBA8SRGB;

    m_pTexture.reset(pDevice->CreateTexture(desc, m_file));
    if (m_pTexture == nullptr)
    {
        stbi_image_free(data);
        return false;
    }

    GfxShaderResourceViewDesc srvDesc;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_file));
    if (m_pSRV == nullptr)
    {
        stbi_image_free(data);
        return false;
    }

    m_pRenderer->UploadTexture(m_pTexture.get(), data, x * y * 4);

    stbi_image_free(data);
    return true;
}
