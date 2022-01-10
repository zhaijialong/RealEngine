#include "common.hlsli"
#include "gpu_scene.hlsli"

cbuffer InstanceCullingConstants : register(b0)
{
#if FIRST_PHASE
    uint c_instanceIndicesAddress;
    uint c_instanceCount;
    uint c_cullingResultUAV;
    uint c_secondPhaseInstanceListUAV;
    uint c_secondPhaseInstanceListCounterUAV;
#else
    uint c_instanceListSRV;
    uint c_instanceListCounterSRV;
    uint c_cullingResultUAV;
#endif
};

[numthreads(64, 1, 1)]
void instance_culling(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.sceneConstantBufferSRV];

#if FIRST_PHASE
    uint instanceCount = c_instanceCount;
    uint instanceIndex = constantBuffer.Load(c_instanceIndicesAddress + sizeof(uint) * dispatchThreadID.x);
#else    
    Buffer<uint> secondPhaseInstanceList = ResourceDescriptorHeap[c_instanceListSRV];
    Buffer<uint> secondPhaseInstanceListCounter = ResourceDescriptorHeap[c_instanceListCounterSRV];
    uint instanceIndex = secondPhaseInstanceList[dispatchThreadID.x];
    uint instanceCount = secondPhaseInstanceListCounter[0];
#endif

    if (dispatchThreadID.x >= instanceCount)
    {
        return;
    }

    InstanceData instanceData = GetInstanceData(instanceIndex);

#if FIRST_PHASE
    Texture2D<float> hzbTexture = ResourceDescriptorHeap[SceneCB.firstPhaseCullingHZBSRV];
#else
    Texture2D<float> hzbTexture = ResourceDescriptorHeap[SceneCB.secondPhaseCullingHZBSRV];
#endif
    uint2 hzbSize = uint2(SceneCB.HZBWidth, SceneCB.HZBHeight);

    bool visible = OcclusionCull(hzbTexture, hzbSize, instanceData.center, instanceData.radius);

    RWBuffer<uint> cullingResultBuffer = ResourceDescriptorHeap[c_cullingResultUAV];
    cullingResultBuffer[instanceIndex] = visible ? 1 : 0;

#if FIRST_PHASE
    if(!visible)
    {
        RWBuffer<uint> secondPhaseInstanceList = ResourceDescriptorHeap[c_secondPhaseInstanceListUAV];
        RWBuffer<uint> secondPhaseInstanceListCounter = ResourceDescriptorHeap[c_secondPhaseInstanceListCounterUAV];

        uint instanceCount;
        InterlockedAdd(secondPhaseInstanceListCounter[0], 1, instanceCount);
    
        secondPhaseInstanceList[instanceCount] = instanceIndex;
    }
#endif
}

cbuffer BuildInstanceCullingCommandConstant : register(b0)
{
    uint c_cullingCommandBufferUAV;
    uint c_objectListCounterBufferSRV;
};

[numthreads(1, 1, 1)]
void build_instance_culling_command() //for second phase
{
    Buffer<uint> secondPhaseInstanceListCounter = ResourceDescriptorHeap[c_objectListCounterBufferSRV];
    uint instanceCount = secondPhaseInstanceListCounter[0];

    RWStructuredBuffer<uint3> commandBuffer = ResourceDescriptorHeap[c_cullingCommandBufferUAV];
    commandBuffer[0] = uint3((instanceCount + 63) / 64, 1, 1);
}

cbuffer BuildMeshletListConstants : register(b1)
{
    uint c_dispatchIndex;
    uint c_cullingResultSRV;
    uint c_originMeshletListAddress;
    uint c_originMeshletCount;
    uint c_meshletListOffset;
    uint c_meshletListBufferUAV;
    uint c_meshletListBufferCounterUAV;
};

[numthreads(64, 1, 1)]
void build_meshlet_list(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_originMeshletCount)
    {
        return;
    }

    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.sceneConstantBufferSRV];
    uint2 dataPerMeshlet = constantBuffer.Load2(c_originMeshletListAddress + sizeof(uint2) * dispatchThreadID.x);

    Buffer<uint> cullingResultBuffer = ResourceDescriptorHeap[c_cullingResultSRV];
    bool visible = (cullingResultBuffer[dataPerMeshlet.x] == 1 ? true : false);

    if(visible)
    {
        RWBuffer<uint2> meshletListBuffer = ResourceDescriptorHeap[c_meshletListBufferUAV];
        RWBuffer<uint> counterBuffer = ResourceDescriptorHeap[c_meshletListBufferCounterUAV];

        uint meshletCount;
        InterlockedAdd(counterBuffer[c_dispatchIndex], 1, meshletCount);

        meshletListBuffer[c_meshletListOffset + meshletCount] = dataPerMeshlet;
    }
}

cbuffer IndirectCommandConstants : register(b0)
{
    uint c_dispatchCount;
    uint c_counterBufferSRV;
    uint c_commandBufferUAV;
};

[numthreads(64, 1, 1)]
void build_indirect_command(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint dispatchIndex = dispatchThreadID.x;
    if (dispatchIndex >= c_dispatchCount)
    {
        return;
    }

    Buffer<uint> counterBuffer = ResourceDescriptorHeap[c_counterBufferSRV];
    RWStructuredBuffer<uint3> commandBuffer = ResourceDescriptorHeap[c_commandBufferUAV];

    uint cullmeshletsCount = counterBuffer[dispatchIndex];
    commandBuffer[dispatchIndex] = uint3((cullmeshletsCount + 31) / 32, 1, 1);
}