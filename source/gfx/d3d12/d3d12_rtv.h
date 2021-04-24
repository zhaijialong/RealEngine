#pragma once

#include "d3d12_header.h"


class D3D12Device;
class IGfxTexture;

class D3D12RTV : public IGfxRenderTargetView
{
public:
	D3D12RTV(D3D12Device* pDevice, IGfxTexture* pTexture, const GfxRenderTargetViewDesc& desc, const std::string& name);
	~D3D12RTV();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_descriptor.cpu_handle; }

	virtual void* GetHandle() const override { return (void*)m_descriptor.gpu_handle.ptr; }

	bool Create();

private:
	D3D12Descriptor m_descriptor;
};