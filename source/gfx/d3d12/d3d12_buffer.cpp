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

void* D3D12Buffer::GetCpuAddress()
{
	return m_pCpuAddress;
}

uint64_t D3D12Buffer::GetGpuAddress()
{
	return m_pBuffer->GetGPUVirtualAddress();
}

bool D3D12Buffer::Create()
{
	D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
	allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

	D3D12_RESOURCE_FLAGS flags = (m_desc.usage & GfxBufferUsageUnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_desc.size, flags);

	D3D12_RESOURCE_STATES initial_state;
	if (allocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		initial_state = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	else if (allocationDesc.HeapType == D3D12_HEAP_TYPE_READBACK)
	{
		initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else
	{
		initial_state = D3D12_RESOURCE_STATE_COMMON;
	}

	HRESULT hr = pAllocator->CreateResource(&allocationDesc, &resourceDesc,
		initial_state, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pBuffer));
	if (FAILED(hr))
	{
		return false;
	}

	std::wstring name_wstr = string_to_wstring(m_name);
	m_pBuffer->SetName(name_wstr.c_str());
	m_pAllocation->SetName(name_wstr.c_str());

	if (m_desc.memory_type != GfxMemoryType::GpuOnly)
	{
		CD3DX12_RANGE readRange(0, 0);
		m_pBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCpuAddress));
	}
	return true;
}
