#pragma once

#include "d3d12_header.h"
#include "../i_gfx_buffer.h"

class D3D12Device;

namespace D3D12MA
{
	class Allocation;
}

class D3D12Buffer : public IGfxBuffer
{
public:
	D3D12Buffer(D3D12Device* pDevice, const GfxBufferDesc& desc, const std::string& name);
	~D3D12Buffer();

	virtual void* GetHandle() const override { return m_pBuffer; }

	virtual void* Map() override;
	virtual void Unmap() override;

	bool Create();
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const;

private:
	ID3D12Resource* m_pBuffer = nullptr;
	D3D12MA::Allocation* m_pAllocation = nullptr;
};
