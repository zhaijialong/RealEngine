#include "d3d12_buffer.h"
#include "d3d12_device.h"
#include "d3d12ma/D3D12MemAlloc.h"

D3D12Buffer::D3D12Buffer(D3D12Device* pDevice, const GfxBufferDesc& desc, const std::string& name)
{
	m_pDevice = pDevice;
	m_desc = desc;
	m_name = name;
}

D3D12Buffer::~D3D12Buffer()
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;
	pDevice->Delete(m_pBuffer);
	pDevice->Delete(m_pAllocation);
}

void* D3D12Buffer::Map()
{
	return nullptr;
}

void D3D12Buffer::Unmap()
{
}

bool D3D12Buffer::Create()
{
	D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
	allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

	D3D12_RESOURCE_FLAGS flags = (m_desc.usage & GfxBufferUsageUnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_desc.size, flags);

	HRESULT hr = pAllocator->CreateResource(&allocationDesc, &resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pBuffer));
	if (FAILED(hr))
	{
		return false;
	}

	std::wstring name_wstr = string_to_wstring(m_name);
	m_pBuffer->SetName(name_wstr.c_str());
	m_pAllocation->SetName(name_wstr.c_str());

	return true;
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12Buffer::GetGpuAddress() const
{
	return m_pBuffer->GetGPUVirtualAddress();
}
