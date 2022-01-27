#pragma once

#include "global_constants.hlsli"

enum class InstanceType : uint
{
    Model = 1 << 0,
};

struct InstanceData
{
    uint instanceType;
    uint indexBufferAddress;
    uint indexStride;
    uint triangleCount;

    uint meshletCount;
    uint meshletBufferAddress;
    uint meshletVerticesBufferAddress;
    uint meshletIndicesBufferAddress;

    uint posBufferAddress;
    uint uvBufferAddress;
    uint normalBufferAddress;
    uint tangentBufferAddress;
    
    uint materialDataAddress;
    uint objectID;
    float scale;
    uint _padding0;
    
    float3 center;
    float radius;
    
    float4x4 mtxWorld;
    float4x4 mtxWorldInverseTranspose;
    float4x4 mtxPrevWorld;

    InstanceType GetInstanceType()
    {
        return (InstanceType)instanceType;
    }
};

#ifndef __cplusplus

InstanceData GetInstanceData(uint instance_id)
{
    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.sceneConstantBufferSRV];
    return constantBuffer.Load<InstanceData>(SceneCB.instanceDataAddress + sizeof(InstanceData) * instance_id);
}

template<typename T>
T LoadSceneBuffer(uint bufferAddress, uint element_id)
{
    ByteAddressBuffer sceneBuffer = ResourceDescriptorHeap[SceneCB.sceneBufferSRV];
    return sceneBuffer.Load<T>(bufferAddress + sizeof(T) * element_id);
}

template<typename T>
T LoadSceneConstantBuffer(uint bufferAddress)
{
    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.sceneConstantBufferSRV];
    return constantBuffer.Load<T>(bufferAddress);
}

uint3 GetPrimitiveIndices(uint instance_id, uint primitive_id)
{
    InstanceData instanceData = GetInstanceData(instance_id);
    
    if (instanceData.indexStride == 2)
    {
        return LoadSceneBuffer<uint16_t3>(instanceData.indexBufferAddress, primitive_id);
    }
    else
    {
        return LoadSceneBuffer<uint3>(instanceData.indexBufferAddress, primitive_id);
    }
}

#endif //__cplusplus