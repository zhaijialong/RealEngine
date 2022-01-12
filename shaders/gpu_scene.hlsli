#pragma once

#include "global_constants.hlsli"

struct InstanceData
{
    uint meshletCount;
    uint meshletBufferAddress;
    uint meshletVerticesBufferAddress;
    uint meshletIndicesBufferAddress;

    uint posBufferAddress;
    uint uvBufferAddress;
    uint normalBufferAddress;
    uint tangentBufferAddress;
    
    uint prevPosBuffer;
    uint objectID;
    uint materialDataAddress;
    float scale;
    
    float3 center;
    float radius;

    uint triangleCount;
    uint3 _padding;
    
    float4x4 mtxWorld;
    float4x4 mtxWorldInverseTranspose;
    float4x4 mtxPrevWorld;
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

#endif //__cplusplus