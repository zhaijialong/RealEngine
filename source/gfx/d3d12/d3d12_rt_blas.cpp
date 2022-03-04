#include "d3d12_rt_blas.h"
#include "d3d12_device.h"
#include "d3d12_buffer.h"

D3D12RayTracingBLAS::D3D12RayTracingBLAS(D3D12Device* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12RayTracingBLAS::~D3D12RayTracingBLAS()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pASBuffer);
    pDevice->Delete(m_pScratchBuffer);
}

bool D3D12RayTracingBLAS::Create()
{
    m_geometries.reserve(m_desc.geometries.size());

    for (size_t i = 0; i < m_desc.geometries.size(); ++i)
    {
        const GfxRayTracingGeometry& geometry = m_desc.geometries[i];

        D3D12_RAYTRACING_GEOMETRY_DESC d3d12_geometry = {};
        d3d12_geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES; //todo : support AABB
        d3d12_geometry.Flags = geometry.opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        d3d12_geometry.Triangles.IndexFormat = dxgi_format(geometry.index_format);
        d3d12_geometry.Triangles.VertexFormat = dxgi_format(geometry.vertex_format);
        d3d12_geometry.Triangles.IndexCount = geometry.index_count;
        d3d12_geometry.Triangles.VertexCount = geometry.vertex_count;
        d3d12_geometry.Triangles.IndexBuffer = geometry.index_buffer->GetGpuAddress() + geometry.index_buffer_offset;
        d3d12_geometry.Triangles.VertexBuffer.StartAddress = geometry.vertex_buffer->GetGpuAddress() + geometry.vertex_buffer_offset;
        d3d12_geometry.Triangles.VertexBuffer.StrideInBytes = geometry.vertex_stride;

        m_geometries.push_back(d3d12_geometry);
    }

    ID3D12Device5* device = (ID3D12Device5*)m_pDevice->GetHandle();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInput;
    buildInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildInput.Flags = d3d12_rt_as_flags(m_desc.flags);
    buildInput.NumDescs = (UINT)m_geometries.size();
    buildInput.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInput.pGeometryDescs = m_geometries.data();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&buildInput, &info);

    //todo : improve memory usage
    CD3DX12_RESOURCE_DESC asBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    CD3DX12_RESOURCE_DESC scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(std::max(info.ScratchDataSizeInBytes, info.UpdateScratchDataSizeInBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    const D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &asBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&m_pASBuffer));
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_pScratchBuffer));

    m_buildDesc.Inputs = buildInput;
    m_buildDesc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    m_buildDesc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();
    m_buildDesc.SourceAccelerationStructureData = 0;

    return true;
}

void D3D12RayTracingBLAS::GetUpdateDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, D3D12_RAYTRACING_GEOMETRY_DESC& geometry, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    RE_ASSERT(m_desc.flags & GfxRayTracingASFlagAllowUpdate);
    RE_ASSERT(m_geometries.size() == 1); //todo : suppport more than 1

    geometry = m_geometries[0];
    geometry.Triangles.VertexBuffer.StartAddress = vertex_buffer->GetGpuAddress() + vertex_buffer_offset;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS updateInputs;
    updateInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    updateInputs.Flags = d3d12_rt_as_flags(m_desc.flags) | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    updateInputs.NumDescs = 1;
    updateInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    updateInputs.pGeometryDescs = &geometry;

    desc.Inputs = updateInputs;
    desc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.SourceAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();
}
