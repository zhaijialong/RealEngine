#pragma once

#include "i_gfx_resource.h"

class IGfxTexture : public IGfxResource
{
public:
	const GfxTextureDesc& GetDesc() const { return m_desc; }

protected:
	GfxTextureDesc m_desc = {};
};
