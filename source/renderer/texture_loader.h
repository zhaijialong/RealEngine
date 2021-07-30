#pragma once

#include "gfx/gfx.h"

class TextureLoader
{
public:
	TextureLoader();
	~TextureLoader();

	bool Load(const std::string& file, bool srgb);

	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	uint32_t GetMipLevels() const { return m_levels; }
	GfxFormat GetFormat() const { return m_format; }

	void* GetData() const { return m_pTextureData; }
	uint32_t GetDataSize() const { return m_textureSize; }

private:
	bool LoadDDS(bool srgb);
	bool LoadSTB(bool srgb);

private:
	uint32_t m_width = 1;
	uint32_t m_height = 1;
	uint32_t m_depth = 1;
	uint32_t m_levels = 1;
	uint32_t m_arraySize = 1;
	GfxFormat m_format = GfxFormat::Unknown;
	GfxTextureType m_type = GfxTextureType::Texture2D;

	bool m_bReleaseData = false;
	void* m_pTextureData = nullptr;
	uint32_t m_textureSize = 0;

	std::vector<uint8_t> m_fileData;
};