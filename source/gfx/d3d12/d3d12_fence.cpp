#include "d3d12_fence.h"
#include "d3d12_device.h"

D3D12Fence::D3D12Fence(D3D12Device* pDevice, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
}

D3D12Fence::~D3D12Fence()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pFence);

    CloseHandle(m_hEvent);
}

void D3D12Fence::Wait(uint64_t value)
{
    if (m_pFence->GetCompletedValue() != value)
    {
        m_pFence->SetEventOnCompletion(value, m_hEvent);
        WaitForSingleObjectEx(m_hEvent, INFINITE, FALSE);
    }
}

void D3D12Fence::Signal(uint64_t value)
{
    m_pFence->Signal(value);
}

bool D3D12Fence::Create()
{
    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    HRESULT hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
    if (FAILED(hr))
    {
        return false;
    }

    m_pFence->SetName(string_to_wstring(m_name).c_str());

    m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    return true;
}
