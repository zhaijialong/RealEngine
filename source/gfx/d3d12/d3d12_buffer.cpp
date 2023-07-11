#include "d3d12_buffer.h"
#include "d3d12_device.h"
#include "d3d12_heap.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "utils/log.h"

D3D12Buffer::D3D12Buffer(D3D12Device* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
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

    D3D12_RESOURCE_FLAGS flags = (m_desc.usage & GfxBufferUsageUnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_DESC1 resourceDesc1 = CD3DX12_RESOURCE_DESC1::Buffer(m_desc.size, flags);

    D3D12_BARRIER_LAYOUT initial_layout = D3D12_BARRIER_LAYOUT_UNDEFINED;

    HRESULT hr;

    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);
        RE_ASSERT(m_desc.size + m_desc.heap_offset <= m_desc.heap->GetDesc().size);

        hr = pAllocator->CreateAliasingResource2((D3D12MA::Allocation*)m_desc.heap->GetHandle(), m_desc.heap_offset,
            &resourceDesc1, initial_layout, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pBuffer));
    }
    else
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
        allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

        hr = pAllocator->CreateResource3(&allocationDesc, 
            &resourceDesc1, initial_layout, nullptr, 0, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pBuffer));
    }

    if (FAILED(hr))
    {
        RE_ERROR("[D3D12Buffer] failed to create {}", m_name);
        return false;
    }

    eastl::wstring name_wstr = string_to_wstring(m_name);
    m_pBuffer->SetName(name_wstr.c_str());
    if (m_pAllocation)
    {
        m_pAllocation->SetName(name_wstr.c_str());
    }

    if (m_desc.memory_type != GfxMemoryType::GpuOnly)
    {
        CD3DX12_RANGE readRange(0, 0);
        m_pBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCpuAddress));
    }
    return true;
}

uint32_t D3D12Buffer::GetRequiredStagingBufferSize() const
{
    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    D3D12_RESOURCE_DESC desc = m_pBuffer->GetDesc();

    uint64_t size;
    pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &size);
    return (uint32_t)size;
}