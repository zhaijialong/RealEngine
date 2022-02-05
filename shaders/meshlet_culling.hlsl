#include "gpu_scene.hlsli"
#include "meshlet.hlsli"
#include "debug.hlsli"
#include "stats.hlsli"

cbuffer MeshletCullingConstant : register(b0)
{
    uint c_meshletListBufferSRV;
    uint c_meshletListCounterSRV;

    uint c_meshletListBufferOffset;
    uint c_dispatchIndex;

    uint c_bFirstPass;
};

groupshared MeshletPayload s_Payload;


bool Cull(Meshlet meshlet, uint instanceIndex, uint meshletIndex)
{
    float3 center = mul(GetInstanceData(instanceIndex).mtxWorld, float4(meshlet.center, 1.0)).xyz;
    float radius = meshlet.radius * GetInstanceData(instanceIndex).scale;

    // 1. frustum culling
    for (uint i = 0; i < 6; ++i)
    {
        if (dot(center, CameraCB.culling.planes[i].xyz) + CameraCB.culling.planes[i].w + radius < 0)
        {
            stats(c_bFirstPass ? STATS_1ST_PHASE_FRUSTUM_CULLED_MESHLET : STATS_2ND_PHASE_FRUSTUM_CULLED_MESHLET, 1);
            return false;
        }
    }
    
#if !DOUBLE_SIDED
    // 2. backface culling
    int16_t4 cone = unpack_s8s16((int8_t4_packed) meshlet.cone);
    float3 axis = cone.xyz / 127.0;
    float cutoff = cone.w / 127.0;
    
    axis = normalize(mul(GetInstanceData(instanceIndex).mtxWorld, float4(axis, 0.0)).xyz);
    float3 view = center - CameraCB.culling.viewPos;
    
    if (dot(view, -axis) >= cutoff * length(view) + radius)
    {
        stats(c_bFirstPass ? STATS_1ST_PHASE_BACKFACE_CULLED_MESHLET : STATS_2ND_PHASE_BACKFACE_CULLED_MESHLET, 1);
        return false;
    }
#endif

    // 3. occlusion culling
    Texture2D<float> hzbTexture = ResourceDescriptorHeap[c_bFirstPass ? SceneCB.firstPhaseCullingHZBSRV : SceneCB.secondPhaseCullingHZBSRV];
    uint2 hzbSize = uint2(SceneCB.HZBWidth, SceneCB.HZBHeight);
    
    if (!OcclusionCull(hzbTexture, hzbSize, center, radius))
    {
        if (c_bFirstPass)
        {
            RWBuffer<uint> counterBuffer = ResourceDescriptorHeap[SceneCB.secondPhaseMeshletsCounterUAV];
            RWStructuredBuffer<uint2> occlusionCulledMeshletsBuffer = ResourceDescriptorHeap[SceneCB.secondPhaseMeshletsListUAV];

            uint culledMeshlets;
            InterlockedAdd(counterBuffer[c_dispatchIndex], 1, culledMeshlets);

            occlusionCulledMeshletsBuffer[c_meshletListBufferOffset + culledMeshlets] = uint2(instanceIndex, meshletIndex);
    
            stats(STATS_1ST_PHASE_OCCLUSION_CULLED_MESHLET, 1);
        }
        else
        {
            stats(STATS_2ND_PHASE_OCCLUSION_CULLED_MESHLET, 1);
        }

        return false;
    }

    stats(c_bFirstPass ? STATS_1ST_PHASE_RENDERED_MESHLET : STATS_2ND_PHASE_RENDERED_MESHLET, 1);
    return true;
}

[numthreads(32, 1, 1)]
void main_as(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    Buffer<uint> counterBuffer = ResourceDescriptorHeap[c_meshletListCounterSRV];
    uint totalMeshletCount = counterBuffer[c_dispatchIndex];
 
    bool visible = false;
    if (dispatchThreadID.x < totalMeshletCount)
    {
        StructuredBuffer<uint2> meshletsListBuffer = ResourceDescriptorHeap[c_meshletListBufferSRV];
        uint2 dataPerMeshlet = meshletsListBuffer[c_meshletListBufferOffset + dispatchThreadID.x];
        uint instanceIndex = dataPerMeshlet.x;
        uint meshletIndex = dataPerMeshlet.y;
        
        Meshlet meshlet = LoadSceneStaticBuffer<Meshlet>(GetInstanceData(instanceIndex).meshletBufferAddress, meshletIndex);
        
        visible = Cull(meshlet, instanceIndex, meshletIndex);
        
        if (c_bFirstPass)
        {
            stats(visible ? STATS_1ST_PHASE_RENDERED_TRIANGLE : STATS_1ST_PHASE_CULLED_TRIANGLE, meshlet.triangleCount);
        }
        else
        {
            stats(visible ? STATS_2ND_PHASE_RENDERED_TRIANGLE : STATS_2ND_PHASE_CULLED_TRIANGLE, meshlet.triangleCount);
        }

        if (visible)
        {
            uint index = WavePrefixCountBits(visible);
            s_Payload.instanceIndices[index] = instanceIndex;
            s_Payload.meshletIndices[index] = meshletIndex;
        }
    }

    uint visibleMeshletCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleMeshletCount, 1, 1, s_Payload);
}
