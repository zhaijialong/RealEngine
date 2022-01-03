#include "gpu_scene.h"
#include "renderer.h"

#include "d3d12ma/D3D12MemAlloc.h"

GpuScene::GpuScene(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    const uint32_t size = 448 * 1024 * 1024;
    m_pSceneBuffer.reset(pRenderer->CreateRawBuffer(nullptr, size, "GpuScene::m_pSceneBuffer"));

    D3D12MA::VIRTUAL_BLOCK_DESC desc = {};
    desc.Size = size;
    D3D12MA::CreateVirtualBlock(&desc, &m_pSceneBufferAllocator);
}

GpuScene::~GpuScene()
{
    m_pSceneBufferAllocator->Release();
}

uint32_t GpuScene::Allocate(uint32_t size, uint32_t alignment)
{
    //todo : resize

    D3D12MA::VIRTUAL_ALLOCATION_DESC desc = {};
    desc.Size = size;
    desc.Alignment = alignment;

    uint64_t address;
    HRESULT hr = m_pSceneBufferAllocator->Allocate(&desc, &address);
    RE_ASSERT(SUCCEEDED(hr));

    return (uint32_t)address;
}

void GpuScene::Free(uint32_t address)
{
    if (address >= m_pSceneBuffer->GetBuffer()->GetDesc().size)
    {
        return;
    }

    m_pSceneBufferAllocator->FreeAllocation(address);
}

void GpuScene::Clear()
{
    m_pSceneBufferAllocator->Clear();
}
