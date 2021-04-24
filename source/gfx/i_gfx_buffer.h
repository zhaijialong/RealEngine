#pragma once

#include "i_gfx_resource.h"

class IGfxBuffer : public IGfxResource
{
public:
	const GfxBufferDesc& GetDesc() const { return m_desc; }

	virtual void* Map() = 0;
	virtual void Unmap() = 0;

protected:
	GfxBufferDesc m_desc = {};
};
