#include "gpu_scene.h"
#include "renderer.h"

#include "d3d12ma/D3D12MemAlloc.h"

#define MAX_CONSTANT_BUFFER_SIZE (8 * 1024 * 1024)

GpuScene::GpuScene(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    const uint32_t static_buffer_size = 448 * 1024 * 1024;
    m_pSceneStaticBuffer.reset(pRenderer->CreateRawBuffer(nullptr, static_buffer_size, "GpuScene::m_pSceneStaticBuffer"));

    const uint32_t animation_buffer_size = 32 * 1024 * 1024;
    m_pSceneAnimationBuffer.reset(pRenderer->CreateRawBuffer(nullptr, animation_buffer_size, "GpuScene::m_pSceneAnimationBuffer", GfxMemoryType::GpuOnly, true));

    D3D12MA::VIRTUAL_BLOCK_DESC desc = {};
    desc.Size = static_buffer_size;
    D3D12MA::CreateVirtualBlock(&desc, &m_pSceneStaticBufferAllocator);

    desc.Size = animation_buffer_size;
    D3D12MA::CreateVirtualBlock(&desc, &m_pSceneAnimationBufferAllocator);

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBuffer[i].reset(pRenderer->CreateRawBuffer(nullptr, MAX_CONSTANT_BUFFER_SIZE, "GpuScene::m_pConstantBuffer", GfxMemoryType::CpuToGpu));
    }
}

GpuScene::~GpuScene()
{
    m_pSceneStaticBufferAllocator->Release();
    m_pSceneAnimationBufferAllocator->Release();
}

uint32_t GpuScene::AllocateStaticBuffer(uint32_t size, uint32_t alignment)
{
    //todo : resize

    D3D12MA::VIRTUAL_ALLOCATION_DESC desc = {};
    desc.Size = size;
    desc.Alignment = alignment;

    uint64_t address;
    HRESULT hr = m_pSceneStaticBufferAllocator->Allocate(&desc, &address);
    RE_ASSERT(SUCCEEDED(hr));

    return (uint32_t)address;
}

void GpuScene::FreeStaticBuffer(uint32_t address)
{
    if (address >= m_pSceneStaticBuffer->GetBuffer()->GetDesc().size)
    {
        return;
    }

    m_pSceneStaticBufferAllocator->FreeAllocation(address);
}

uint32_t GpuScene::AllocateAnimationBuffer(uint32_t size, uint32_t alignment)
{
    //todo : resize

    D3D12MA::VIRTUAL_ALLOCATION_DESC desc = {};
    desc.Size = size;
    desc.Alignment = alignment;

    uint64_t address;
    HRESULT hr = m_pSceneAnimationBufferAllocator->Allocate(&desc, &address);
    RE_ASSERT(SUCCEEDED(hr));

    return (uint32_t)address;
}

void GpuScene::FreeAnimationBuffer(uint32_t address)
{
    if (address >= m_pSceneStaticBuffer->GetBuffer()->GetDesc().size)
    {
        return;
    }

    m_pSceneAnimationBufferAllocator->FreeAllocation(address);
}

void GpuScene::Update()
{
    uint32_t instance_count = (uint32_t)m_instanceData.size();
    m_instanceDataAddress = m_pRenderer->AllocateSceneConstant(m_instanceData.data(), sizeof(InstanceData) * instance_count);

    uint32_t rt_instance_count = (uint32_t)m_raytracingInstances.size();
    if (m_pSceneTLAS == nullptr || m_pSceneTLAS->GetDesc().instance_count < rt_instance_count)
    {
        GfxRayTracingTLASDesc desc;
        desc.instance_count = max(rt_instance_count, 1);
        desc.flags = GfxRayTracingASFlagPreferFastBuild;

        IGfxDevice* device = m_pRenderer->GetDevice();
        m_pSceneTLAS.reset(device->CreateRayTracingTLAS(desc, "GpuScene::m_pSceneTLAS"));

        GfxShaderResourceViewDesc srvDesc;
        srvDesc.type = GfxShaderResourceViewType::RayTracingTLAS;
        m_pSceneTLASSRV.reset(device->CreateShaderResourceView(m_pSceneTLAS.get(), srvDesc, "GpuScene::m_pSceneTLAS"));
    }
}

void GpuScene::BuildRayTracingAS(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "BuildTLAS");

    pCommandList->BuildRayTracingTLAS(m_pSceneTLAS.get(), m_raytracingInstances.data(), (uint32_t)m_raytracingInstances.size());
    pCommandList->UavBarrier(m_pSceneTLAS.get());

    m_raytracingInstances.clear();
}

uint32_t GpuScene::AllocateConstantBuffer(uint32_t size)
{
    RE_ASSERT(m_nConstantBufferOffset + size <= MAX_CONSTANT_BUFFER_SIZE);
    RE_ASSERT(size % 4 == 0);

    uint32_t address = m_nConstantBufferOffset;
    m_nConstantBufferOffset += size;

    return address;
}

uint32_t GpuScene::AddInstance(const InstanceData& data, IGfxRayTracingBLAS* blas, GfxRayTracingInstanceFlag flags)
{
    m_instanceData.push_back(data);
    uint32_t instance_id = (uint32_t)m_instanceData.size() - 1;

    if (blas)
    {
        float4x4 transform = transpose(data.mtxWorld);

        GfxRayTracingInstance instance;
        instance.blas = blas;
        memcpy(instance.transform, &transform, sizeof(float) * 12);
        instance.instance_id = instance_id;
        instance.instance_mask = 0xFF; //todo
        instance.flags = flags;

        m_raytracingInstances.push_back(instance);
    }

    return instance_id;
}

void GpuScene::ResetFrameData()
{
    m_instanceData.clear();
    m_nConstantBufferOffset = 0;
}

void GpuScene::BeginAnimationUpdate(IGfxCommandList* pCommandList)
{
    pCommandList->ResourceBarrier(m_pSceneAnimationBuffer->GetBuffer(), 0, GfxResourceState::ShaderResourceNonPS, GfxResourceState::UnorderedAccess);
}

void GpuScene::EndAnimationUpdate(IGfxCommandList* pCommandList)
{
    pCommandList->ResourceBarrier(m_pSceneAnimationBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceNonPS);
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
