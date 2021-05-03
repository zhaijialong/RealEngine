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
	virtual uint32_t GetRequiredStagingBufferSize() const override;
	virtual uint32_t GetRowPitch(uint32_t mip_level) const override;

	bool Create();
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(uint32_t mip_slice, uint32_t array_slice);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(uint32_t mip_slice, uint32_t array_slice);

private:
	ID3D12Resource* m_pTexture = nullptr;
	D3D12MA::Allocation* m_pAllocation = nullptr;

	std::vector<D3D12Descriptor> m_RTV;
	std::vector<D3D12Descriptor> m_DSV;

private:
	friend class D3D12Swapchain;
};
