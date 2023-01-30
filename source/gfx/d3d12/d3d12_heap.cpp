#include "d3d12_heap.h"
#include "d3d12_device.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "utils/log.h"

D3D12Heap::D3D12Heap(D3D12Device* pDevice, const GfxHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Heap::~D3D12Heap()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pAllocation);
}

void* D3D12Heap::GetHandle() const
{
    return m_pAllocation;
}

ID3D12Heap* D3D12Heap::GetHeap() const
{
    return m_pAllocation->GetHeap();
}

bool D3D12Heap::Create()
{
    RE_ASSERT(m_desc.size % (64 * 1024) == 0);

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
    allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;

    D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = {};
    allocationInfo.SizeInBytes = m_desc.size;
    allocationInfo.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();
    HRESULT hr = pAllocator->AllocateMemory(&allocationDesc, &allocationInfo, &m_pAllocation);
    if (FAILED(hr))
    {
        RE_ERROR("[D3D12Heap] failed to create {}", m_name);
        return false;
    }

    m_pAllocation->SetName(string_to_wstring(m_name).c_str());

    return true;
}
