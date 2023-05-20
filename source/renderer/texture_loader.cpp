#include "texture_loader.h"
#include "utils/assert.h"
#include "stb/stb_image.h"
#include "ddspp/ddspp.h"
#include <fstream>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb//stb_image_resize.h"

static inline GfxTextureType get_texture_type(ddspp::TextureType type, bool array)
{
    switch (type)
    {
    case ddspp::Texture2D:
        return array ? GfxTextureType::Texture2DArray : GfxTextureType::Texture2D;
    case ddspp::Texture3D:
        return GfxTextureType::Texture3D;
    case ddspp::Cubemap:
        return array ? GfxTextureType::TextureCubeArray : GfxTextureType::TextureCube;
    case ddspp::Texture1D:
    default:
        RE_ASSERT(false);
        return GfxTextureType::Texture2D;
    }
}

static inline GfxFormat get_texture_format(ddspp::DXGIFormat format, bool srgb)
{
    switch(format)
    {
    case ddspp::UNKNOWN:
        return GfxFormat::Unknown;
    case ddspp::R32G32B32A32_FLOAT:
        return GfxFormat::RGBA32F;
    case ddspp::R32G32B32A32_UINT:
        return GfxFormat::RGBA32UI;
    case ddspp::R32G32B32A32_SINT:
        return GfxFormat::RGBA32SI;
    case ddspp::R16G16B16A16_FLOAT:
        return GfxFormat::RGBA16F;
    case ddspp::R16G16B16A16_UINT:
        return GfxFormat::RGBA16UI;
    case ddspp::R16G16B16A16_SINT:
        return GfxFormat::RGBA16SI;
    case ddspp::R16G16B16A16_UNORM:
        return GfxFormat::RGBA16UNORM;
    case ddspp::R16G16B16A16_SNORM:
        return GfxFormat::RGBA16SNORM;
    case ddspp::R8G8B8A8_UINT:
        return GfxFormat::RGBA8UI;
    case ddspp::R8G8B8A8_SINT:
        return GfxFormat::RGBA8SI;
    case ddspp::R8G8B8A8_UNORM:
        return srgb ? GfxFormat::RGBA8SRGB : GfxFormat::RGBA8UNORM;
    case ddspp::R8G8B8A8_SNORM:
        return GfxFormat::RGBA8SNORM;
    case ddspp::R8G8B8A8_UNORM_SRGB:
        return GfxFormat::RGBA8SRGB;
    case ddspp::B8G8R8A8_UNORM:
        return srgb ? GfxFormat::BGRA8SRGB : GfxFormat::BGRA8UNORM;
    case ddspp::B8G8R8A8_UNORM_SRGB:
        return GfxFormat::BGRA8SRGB;
    case ddspp::R9G9B9E5_SHAREDEXP:
        return GfxFormat::RGB9E5;
    case ddspp::R32G32_FLOAT:
        return GfxFormat::RG32F;
    case ddspp::R32G32_UINT:
        return GfxFormat::RG32UI;
    case ddspp::R32G32_SINT:
        return GfxFormat::RG32SI;
    case ddspp::R16G16_FLOAT:
        return GfxFormat::RG16F;
    case ddspp::R16G16_UINT:
        return GfxFormat::RG16UI;
    case ddspp::R16G16_SINT:
        return GfxFormat::RG16SI;
    case ddspp::R16G16_UNORM:
        return GfxFormat::RG16UNORM;
    case ddspp::R16G16_SNORM:
        return GfxFormat::RG16SNORM;
    case ddspp::R8G8_UINT:
        return GfxFormat::RG8UI;
    case ddspp::R8G8_SINT:
        return GfxFormat::RG8SI;
    case ddspp::R8G8_UNORM:
        return GfxFormat::RG8UNORM;
    case ddspp::R8G8_SNORM:
        return GfxFormat::RG8SNORM;
    case ddspp::R32_FLOAT:
        return GfxFormat::R32F;
    case ddspp::R32_UINT:
        return GfxFormat::R32UI;
    case ddspp::R32_SINT:
        return GfxFormat::R32SI;
    case ddspp::R16_FLOAT:
        return GfxFormat::R16F;
    case ddspp::R16_UINT:
        return GfxFormat::R16UI;
    case ddspp::R16_SINT:
        return GfxFormat::R16SI;
    case ddspp::R16_UNORM:
        return GfxFormat::R16UNORM;
    case ddspp::R16_SNORM:
        return GfxFormat::R16SNORM;
    case ddspp::R8_UINT:
        return GfxFormat::R8UI;
    case ddspp::R8_SINT:
        return GfxFormat::R8SI;
    case ddspp::R8_UNORM:
        return GfxFormat::R8UNORM;
    case ddspp::R8_SNORM:
        return GfxFormat::R8SNORM;
    case ddspp::BC1_UNORM:
        return srgb ? GfxFormat::BC1SRGB : GfxFormat::BC1UNORM;
    case ddspp::BC1_UNORM_SRGB:
        return GfxFormat::BC1SRGB;
    case ddspp::BC2_UNORM:
        return srgb ? GfxFormat::BC2SRGB : GfxFormat::BC2UNORM;
    case ddspp::BC2_UNORM_SRGB:
        return GfxFormat::BC2SRGB;
    case ddspp::BC3_UNORM:
        return srgb ? GfxFormat::BC3SRGB : GfxFormat::BC3UNORM;
    case ddspp::BC3_UNORM_SRGB:
        return GfxFormat::BC3SRGB;
    case ddspp::BC4_UNORM:
        return GfxFormat::BC4UNORM;
    case ddspp::BC4_SNORM:
        return GfxFormat::BC4SNORM;
    case ddspp::BC5_UNORM:
        return GfxFormat::BC5UNORM;
    case ddspp::BC5_SNORM:
        return GfxFormat::BC5SNORM;
    case ddspp::BC6H_UF16:
        return GfxFormat::BC6U16F;
    case ddspp::BC6H_SF16:
        return GfxFormat::BC6S16F;
    case ddspp::BC7_UNORM:
        return srgb ? GfxFormat::BC7SRGB : GfxFormat::BC7UNORM;
    case ddspp::BC7_UNORM_SRGB:
        return GfxFormat::BC7SRGB;
    default:
        RE_ASSERT(false);
        return GfxFormat::Unknown;
    }
}

TextureLoader::TextureLoader()
{
}

TextureLoader::~TextureLoader()
{
    if (m_pDecompressedData)
    {
        stbi_image_free(m_pDecompressedData);
    }
}

bool TextureLoader::Load(const eastl::string& file, bool srgb)
{
    std::ifstream is;
    is.open(file.c_str(), std::ios::binary);
    if (is.fail())
    {
        return false;
    }

    is.seekg(0, std::ios::end);
    uint32_t length = (uint32_t)is.tellg();
    is.seekg(0, std::ios::beg);

    m_fileData.resize(length);
    char* buffer = (char*)m_fileData.data();

    is.read(buffer, length);
    is.close();

    if (file.find(".dds") != eastl::string::npos)
    {
        return LoadDDS(srgb);
    }
    else
    {
        return LoadSTB(srgb);
    }
}

bool TextureLoader::LoadDDS(bool srgb)
{
    uint8_t* data = m_fileData.data();

    ddspp::Descriptor desc;
    ddspp::Result result = ddspp::decode_header((unsigned char*)data, desc);
    if (result != ddspp::Success)
    {
        return false;
    }

    m_width = desc.width;
    m_height = desc.height;
    m_depth = desc.depth;
    m_levels = desc.numMips;
    m_arraySize = desc.arraySize;
    m_type = get_texture_type(desc.type, desc.arraySize > 1);
    m_format = get_texture_format(desc.format, srgb);

    m_pTextureData = data + desc.headerSize;
    m_textureSize = (uint32_t)m_fileData.size() - desc.headerSize;

    return true;
}


bool TextureLoader::LoadSTB(bool srgb)
{
    int x, y, comp;
    stbi_info_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp);

    bool isHDR = stbi_is_hdr_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size());
    bool is16Bits = stbi_is_16_bit_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size());

    if (isHDR)
    {
        m_pDecompressedData = stbi_loadf_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, 0);

        switch (comp)
        {
        case 1:
            m_format = GfxFormat::R32F;
            break;
        case 2:
            m_format = GfxFormat::RG32F;
            break;
        case 3:
            m_format = GfxFormat::RGB32F;
            break;
        case 4:
        default:
            m_format = GfxFormat::RGBA32F;
            break;
        }
    }
    else if (is16Bits)
    {
        m_pDecompressedData = stbi_load_16_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, 0);

        switch (comp)
        {
        case 1:
            m_format = GfxFormat::R16UNORM;
            break;
        case 2:
            m_format = GfxFormat::RG16UNORM;
            break;
        case 4:
        default:
            m_format = GfxFormat::RGBA16UNORM;
            break;
        }
    }
    else
    {
        m_pDecompressedData = stbi_load_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, 0);

        switch (comp)
        {
        case 1:
            m_format = GfxFormat::R8UNORM;
            break;
        case 2:
            m_format = GfxFormat::RG8UNORM;
            break;
        case 4:
        default:
            m_format = srgb ? GfxFormat::RGBA8SRGB : GfxFormat::RGBA8UNORM;
            break;
        }
    }

    if (m_pDecompressedData == nullptr)
    {
        return false;
    }

    m_width = x;
    m_height = y;
    m_textureSize = GetFormatRowPitch(m_format, x) * y;

    return true;
}

bool TextureLoader::Resize(uint32_t width, uint32_t height)
{
    if (m_pDecompressedData == nullptr)
    {
        return false;
    }

    unsigned char* output_data = (unsigned char*)malloc(4 * width * height);
    int result = stbir_resize_uint8((const unsigned char*)m_pDecompressedData, m_width, m_height, 0,
        output_data, width, height, 0, 4);
    if (!result || output_data == nullptr)
    {
        return false;
    }

    stbi_image_free(m_pDecompressedData);

    m_width = width;
    m_height = height;
    m_pDecompressedData = output_data;

    return true;
}