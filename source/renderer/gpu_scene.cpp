#include "gpu_scene.h"
#include "renderer.h"

#define MAX_CONSTANT_BUFFER_SIZE (8 * 1024 * 1024)
#define ALLOCATION_ALIGNMENT (4)

GpuScene::GpuScene(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    const uint32_t static_buffer_size = 448 * 1024 * 1024;
    m_pSceneStaticBuffer.reset(pRenderer->CreateRawBuffer(nullptr, static_buffer_size, "GpuScene::m_pSceneStaticBuffer"));
    m_pSceneStaticBufferAllocator = eastl::make_unique<OffsetAllocator::Allocator>(static_buffer_size);

    const uint32_t animation_buffer_size = 32 * 1024 * 1024;
    m_pSceneAnimationBuffer.reset(pRenderer->CreateRawBuffer(nullptr, animation_buffer_size, "GpuScene::m_pSceneAnimationBuffer", GfxMemoryType::GpuOnly, true));
    m_pSceneAnimationBufferAllocator = eastl::make_unique<OffsetAllocator::Allocator>(animation_buffer_size);

    for (int i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBuffer[i].reset(pRenderer->CreateRawBuffer(nullptr, MAX_CONSTANT_BUFFER_SIZE, "GpuScene::m_pConstantBuffer", GfxMemoryType::CpuToGpu));
    }
}

GpuScene::~GpuScene()
{
}

OffsetAllocator::Allocation GpuScene::AllocateStaticBuffer(uint32_t size)
{
    //todo : resize

    return m_pSceneStaticBufferAllocator->allocate(RoundUpPow2(size, ALLOCATION_ALIGNMENT));
}

void GpuScene::FreeStaticBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.offset >= m_pSceneStaticBuffer->GetBuffer()->GetDesc().size)
    {
        return;
    }

    m_pSceneStaticBufferAllocator->free(allocation);
}

OffsetAllocator::Allocation GpuScene::AllocateAnimationBuffer(uint32_t size)
{
    //todo : resize

    return m_pSceneAnimationBufferAllocator->allocate(RoundUpPow2(size, ALLOCATION_ALIGNMENT));
}

void GpuScene::FreeAnimationBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.offset >= m_pSceneStaticBuffer->GetBuffer()->GetDesc().size)
    {
        return;
    }

    m_pSceneAnimationBufferAllocator->free(allocation);
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
    pCommandList->GlobalBarrier(GfxAccessMaskAS, GfxAccessMaskSRV);

    m_raytracingInstances.clear();
}

uint32_t GpuScene::AllocateConstantBuffer(uint32_t size)
{
    RE_ASSERT(m_nConstantBufferOffset + size <= MAX_CONSTANT_BUFFER_SIZE);

    uint32_t address = m_nConstantBufferOffset;
    m_nConstantBufferOffset += RoundUpPow2(size, ALLOCATION_ALIGNMENT);

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
    pCommandList->BufferBarrier(m_pSceneAnimationBuffer->GetBuffer(), GfxAccessVertexShaderSRV | GfxAccessASRead, GfxAccessComputeUAV);
}

void GpuScene::EndAnimationUpdate(IGfxCommandList* pCommandList)
{
    pCommandList->BufferBarrier(m_pSceneAnimationBuffer->GetBuffer(), GfxAccessComputeUAV, GfxAccessVertexShaderSRV | GfxAccessASRead);
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
