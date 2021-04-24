#pragma once

#include "gfx_define.h"
#include <string>

class IGfxResource;
class IGfxBuffer;
class IGfxTexture;
class IGfxFence;
class IGfxSwapchain;
class IGfxCommandList;
class IGfxRenderTargetView;
class IGfxDepthStencilView;
class IGfxContantBufferView;
class IGfxShaderResourceView;
class IGfxUnorderedAccessView;

class IGfxDevice
{
public:
	virtual ~IGfxDevice() {}

	virtual void* GetHandle() const = 0;

	virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const std::string& name) = 0;
	virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const std::string& name) = 0;
	virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, void* window_handle, const std::string& name) = 0;
	virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const std::string& name) = 0;
	virtual IGfxFence* CreateFence(const std::string& name) = 0;

	virtual IGfxRenderTargetView* CreateRenderTargetView(IGfxTexture* texture, const GfxRenderTargetViewDesc& desc, const std::string& name) = 0;
	virtual IGfxDepthStencilView* CreateDepthStencilView(IGfxTexture* texture, const GfxDepthStencilViewDesc& desc, const std::string& name) = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual uint64_t GetFrameID() const = 0;
};