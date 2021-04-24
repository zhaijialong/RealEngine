#pragma once

#include "d3d12_header.h"
#include "../i_gfx_texture.h"

class D3D12Device;

namespace D3D12MA
{
	class Allocation;
}

class D3D12Texture : public IGfxTexture
{
public:
	D3D12Texture(D3D12Device* pDevice, const GfxTextureDesc& desc, const std::string& name);
	~D3D12Texture();

	virtual void* GetHandle() const override { return m_pTexture; }

	bool Create();

private:
	ID3D12Resource* m_pTexture = nullptr;
	D3D12MA::Allocation* m_pAllocation = nullptr;

private:
	friend class D3D12Swapchain;
};
