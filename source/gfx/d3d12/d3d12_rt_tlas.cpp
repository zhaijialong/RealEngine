#include "d3d12_rt_tlas.h"
#include "d3d12_rt_blas.h"
#include "d3d12_device.h"
#include "utils/math.h"
#include "d3d12ma/D3D12MemAlloc.h"

D3D12RayTracingTLAS::D3D12RayTracingTLAS(D3D12Device* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12RayTracingTLAS::~D3D12RayTracingTLAS()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pASBuffer);
    pDevice->Delete(m_pASAllocation);
    pDevice->Delete(m_pScratchBuffer);
    pDevice->Delete(m_pScratchAllocation);
    pDevice->Delete(m_pInstanceBuffer);
    pDevice->Delete(m_pInstanceAllocation);
}

bool D3D12RayTracingTLAS::Create()
{
    ID3D12Device5* device = (ID3D12Device5*)m_pDevice->GetHandle();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = m_desc.instance_count;
    prebuildDesc.Flags = d3d12_rt_as_flags(m_desc.flags);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    //todo : better memory allocation and compaction like what RTXMU does
    D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    CD3DX12_RESOURCE_DESC asBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    CD3DX12_RESOURCE_DESC scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    pAllocator->CreateResource(&allocationDesc, &asBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, &m_pASAllocation, IID_PPV_ARGS(&m_pASBuffer));
    pAllocator->CreateResource(&allocationDesc, &scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pScratchAllocation, IID_PPV_ARGS(&m_pScratchBuffer));

    allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
    m_nInstanceBufferSize = RoundUpPow2(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * m_desc.instance_count, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT) * GFX_MAX_INFLIGHT_FRAMES;
    CD3DX12_RESOURCE_DESC instanceBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_nInstanceBufferSize);
    pAllocator->CreateResource(&allocationDesc, &instanceBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pInstanceAllocation, IID_PPV_ARGS(&m_pInstanceBuffer));

    m_pASBuffer->SetName(string_to_wstring(m_name).c_str());
    m_pASAllocation->SetName(string_to_wstring(m_name).c_str());

    CD3DX12_RANGE readRange(0, 0);
    m_pInstanceBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pInstanceBufferCpuAddress));

    return true;
}

void D3D12RayTracingTLAS::GetBuildDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    RE_ASSERT(instance_count <= m_desc.instance_count);

    if (m_nCurrentInstanceBufferOffset + sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instance_count > m_nInstanceBufferSize)
    {
        m_nCurrentInstanceBufferOffset = 0;
    }

    desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    desc.Inputs.InstanceDescs = m_pInstanceBuffer->GetGPUVirtualAddress() + m_nCurrentInstanceBufferOffset;
    desc.Inputs.NumDescs = instance_count;
    desc.Inputs.Flags = d3d12_rt_as_flags(m_desc.flags);
    desc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();

    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = (D3D12_RAYTRACING_INSTANCE_DESC*)((char*)m_pInstanceBufferCpuAddress + m_nCurrentInstanceBufferOffset);
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        memcpy(instanceDescs[i].Transform, instances[i].transform, sizeof(float) * 12);
        instanceDescs[i].InstanceID = instances[i].instance_id;
        instanceDescs[i].InstanceMask = instances[i].instance_mask;
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0; //DXR 1.1 does't need it
        instanceDescs[i].Flags = d3d12_rt_instance_flags(instances[i].flags);
        instanceDescs[i].AccelerationStructure = ((D3D12RayTracingBLAS*)instances[i].blas)->GetGpuAddress();
    }

    m_nCurrentInstanceBufferOffset += sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instance_count;
    m_nCurrentInstanceBufferOffset = RoundUpPow2(m_nCurrentInstanceBufferOffset, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);
}
