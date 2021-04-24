#pragma once

#include "d3d12_header.h"
#include "../i_gfx_descriptor.h"

class D3D12Device;
class IGfxTexture;

class D3D12DSV : public IGfxDepthStencilView
{
public:
	D3D12DSV(D3D12Device* pDevice, IGfxTexture* pTexture, const GfxDepthStencilViewDesc& desc, const std::string& name);
	~D3D12DSV();

	virtual void* GetHandle() const override { return (void*)m_descriptor.gpu_handle.ptr; }

	bool Create();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_descriptor.cpu_handle; }

private:
	D3D12Descriptor m_descriptor;
};