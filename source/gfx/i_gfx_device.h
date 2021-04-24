#pragma once

#include "gfx_define.h"
#include <string>

class IGfxBuffer;
class IGfxTexture;
class IGfxFence;
class IGfxSwapchain;
class IGfxCommandList;

class IGfxDevice
{
public:
	virtual ~IGfxDevice() {}

	virtual void* GetHandle() const = 0;

	virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, const std::string& name) = 0;
	virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const std::string& name) = 0;
	virtual IGfxFence* CreateFence(const std::string& name) = 0;
	virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const std::string& name) = 0;
	virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const std::string& name) = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual uint64_t GetFrameID() const = 0;
};