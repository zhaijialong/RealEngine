#pragma once

#include "d3d12_header.h"
#include "../gfx_buffer.h"

class D3D12Device;
class D3D12Heap;

namespace D3D12MA
{
    class Allocation;
}

class D3D12Buffer : public IGfxBuffer
{
public:
    D3D12Buffer(D3D12Device* pDevice, const GfxBufferDesc& desc, const eastl::string& name);
    ~D3D12Buffer();

    virtual void* GetHandle() const override { return m_pBuffer; }
    virtual void* GetCpuAddress() override;
    virtual uint64_t GetGpuAddress() override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual void* GetSharedHandle() const override { return m_sharedHandle; }

    bool Create();

private:
    ID3D12Resource* m_pBuffer = nullptr;
    D3D12MA::Allocation* m_pAllocation = nullptr;
    void* m_pCpuAddress = nullptr;

    HANDLE m_sharedHandle = 0;
};
