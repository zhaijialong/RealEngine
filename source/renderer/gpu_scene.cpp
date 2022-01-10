#include "gpu_scene.h"
#include "renderer.h"

#include "d3d12ma/D3D12MemAlloc.h"

#define MAX_CONSTANT_BUFFER_SIZE (8 * 1024 * 1024)

GpuScene::GpuScene(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    const uint32_t size = 448 * 1024 * 1024;
    m_pSceneBuffer.reset(pRenderer->CreateRawBuffer(nullptr, size, "GpuScene::m_pSceneBuffer"));

    D3D12MA::VIRTUAL_BLOCK_DESC desc = {};
    desc.Size = size;
    D3D12MA::CreateVirtualBlock(&desc, &m_pSceneBufferAllocator);

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBuffer[i].reset(pRenderer->CreateRawBuffer(nullptr, MAX_CONSTANT_BUFFER_SIZE, "GpuScene::m_pConstantBuffer", GfxMemoryType::CpuToGpu));
    }
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

void GpuScene::Update()
{
    m_instanceDataAddress = m_pRenderer->AllocateSceneConstant(m_instanceData.data(), sizeof(InstanceData) * (uint32_t)m_instanceData.size());
}

uint32_t GpuScene::AllocateConstantBuffer(uint32_t size)
{
    RE_ASSERT(m_nConstantBufferOffset + size <= MAX_CONSTANT_BUFFER_SIZE);
    RE_ASSERT(size % 4 == 0);

    uint32_t address = m_nConstantBufferOffset;
    m_nConstantBufferOffset += size;

    return address;
}

uint32_t GpuScene::AddInstance(const InstanceData& data)
{
    m_instanceData.push_back(data);
    return (uint32_t)m_instanceData.size() - 1;
}

void GpuScene::ResetFrameData()
{
    m_instanceData.clear();
    m_nConstantBufferOffset = 0;
}

IGfxBuffer* GpuScene::GetSceneConstantBuffer() const
{
    uint32_t frame_index = m_pRenderer->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    return m_pConstantBuffer[frame_index]->GetBuffer();
}

IGfxDescriptor* GpuScene::GetSceneConstantSRV() const
{
    uint32_t frame_index = m_pRenderer->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    return m_pConstantBuffer[frame_index]->GetSRV();
}
