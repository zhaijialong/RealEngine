#include "texture_loader.h"
#include "utils/assert.h"
#include "stb/stb_image.h"
#include "d3d12/dxgiformat.h"
#include "dds/dds.h"
#include <fstream>

using namespace DirectX;

TextureLoader::TextureLoader()
{
}

TextureLoader::~TextureLoader()
{
    if (m_bReleaseData)
    {
        free(m_pTextureData);
    }
}

bool TextureLoader::Load(const std::string& file, bool srgb)
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

    if (file.find(".dds") != std::string::npos)
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

	uint32_t fourcc = *(uint32_t*)data;
	if (fourcc != DDS_MAGIC)
	{
		return false;
	}

	DDS_HEADER header = *(DDS_HEADER*)(data + sizeof(uint32_t));
	if (header.size != sizeof(DDS_HEADER) || header.ddspf.size != sizeof(DDS_PIXELFORMAT))
	{
		return false;
	}

	m_width = header.width;
	m_height = header.height;
	m_depth = header.depth;
	m_levels = header.mipMapCount;

	// Check for DX10 extension
	if ((header.ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header.ddspf.fourCC))
	{
		DDS_HEADER_DXT10 dxt10_header = *(DDS_HEADER_DXT10*)(data + sizeof(uint32_t) + sizeof(DDS_HEADER));

        m_arraySize = 6;// dxt10_header.arraySize;
        m_type = GfxTextureType::TextureCube;
        m_format = GfxFormat::RGBA16F;

        switch (dxt10_header.resourceDimension)
        {
        case DDS_DIMENSION_TEXTURE2D:
            break;
        case DDS_DIMENSION_TEXTURE3D:
            break;
        default:
            break;
        }
		//m_format = dxt10_header.dxgiFormat;

        uint32_t header_size = sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
		m_pTextureData = data + header_size;
        m_textureSize = (uint32_t)m_fileData.size() - header_size;
	}
	else
	{
		//todo
		RE_ASSERT(false);
	}

    return true;
}

bool TextureLoader::LoadSTB(bool srgb)
{
    int x, y, comp;
    m_pTextureData = stbi_load_from_memory((stbi_uc*)m_fileData.data(), (int)m_fileData.size(), &x, &y, &comp, STBI_rgb_alpha);
    if (m_pTextureData == nullptr)
    {
        return false;
    }

    m_width = x;
    m_height = y;
    m_format = srgb ? GfxFormat::RGBA8SRGB : GfxFormat::RGBA8UNORM;
    m_bReleaseData = true;
    m_textureSize = x * y * 4;    

    return true;
}
