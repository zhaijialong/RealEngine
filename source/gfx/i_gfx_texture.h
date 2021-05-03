#pragma once

#include "i_gfx_resource.h"

class IGfxTexture : public IGfxResource
{
public:
	const GfxTextureDesc& GetDesc() const { return m_desc; }

	virtual uint32_t GetRequiredStagingBufferSize() const = 0;
	virtual uint32_t GetRowPitch(uint32_t mip_level) const = 0;

protected:
	GfxTextureDesc m_desc = {};
};
