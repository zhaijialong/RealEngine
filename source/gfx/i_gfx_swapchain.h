#pragma once

#include "i_gfx_resource.h"

class IGfxTexture;
class IGfxRenderTargetView;

class IGfxSwapchain : public IGfxResource
{
public:
	virtual ~IGfxSwapchain() {}

	virtual bool Present() = 0;
	virtual bool Resize(uint32_t width, uint32_t height) = 0;
	virtual const GfxSwapchainDesc& GetDesc() const = 0;
	virtual IGfxTexture* GetBackBuffer() const = 0;
	virtual IGfxRenderTargetView* GetRenderTargetView() const = 0;
};