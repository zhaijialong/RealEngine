#include "model.hlsli"
#include "debug.hlsli"
#include "stats.hlsli"

cbuffer _ : register(b0)
{
    uint c_dataPerMeshletAddress;    
    uint c_occlusionCulledMeshletsBufferOffset;
    uint c_dispatchIndex;

    uint c_bFirstPass;
    uint c_mergedMeshletCount;
};

uint2 LoadDataPerMeshlet(uint meshlet_index)
{
    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.sceneConstantBufferSRV];
    return constantBuffer.Load2(c_dataPerMeshletAddress + sizeof(uint2) * meshlet_index);
}

struct Meshlet
{
    float3 center;
    float radius;
    
    uint cone; //axis + cutoff, rgba8snorm
    
    uint vertexCount;
    uint triangleCount;
    
    uint vertexOffset;
    uint triangleOffset;
};

struct Payload
{
    uint sceneConstantAddress[32];
    uint meshletIndices[32];
};

groupshared Payload s_Payload;


bool Cull(Meshlet meshlet, uint meshletIndex, uint sceneConstantAddress)
{
    float3 center = mul(GetModelConstant(sceneConstantAddress).mtxWorld, float4(meshlet.center, 1.0)).xyz;
    float radius = meshlet.radius * GetModelConstant(sceneConstantAddress).scale;

    if (c_bFirstPass)
    {
        // 1. frustum culling
        for (uint i = 0; i < 6; ++i)
        {
            if (dot(center, CameraCB.culling.planes[i].xyz) + CameraCB.culling.planes[i].w + radius < 0)
            {
                stats(STATS_FRUSTUM_CULLED_MESHLET, 1);
                return false;
            }
        }
    
#if !DOUBLE_SIDED
        // 2. backface culling
        int16_t4 cone = unpack_s8s16((int8_t4_packed) meshlet.cone);
        float3 axis = cone.xyz / 127.0;
        float cutoff = cone.w / 127.0;
    
        axis = normalize(mul(GetModelConstant(sceneConstantAddress).mtxWorld, float4(axis, 0.0)).xyz);
        float3 view = center - CameraCB.culling.viewPos;
    
        if (dot(view, -axis) >= cutoff * length(view) + radius)
        {
            stats(STATS_BACKFACE_CULLED_MESHLET, 1);
            return false;
        }
#endif
    }

    // 3. occlusion culling
    Texture2D<float> hzbTexture = ResourceDescriptorHeap[c_bFirstPass ? SceneCB.firstPhaseCullingHZBSRV : SceneCB.secondPhaseCullingHZBSRV];
    uint2 hzbSize = uint2(SceneCB.HZBWidth, SceneCB.HZBHeight);
    
    if (!OcclusionCull(hzbTexture, hzbSize, center, radius))
    {
        if (c_bFirstPass)
        {
            RWByteAddressBuffer counterBuffer = ResourceDescriptorHeap[SceneCB.occlusionCulledMeshletsCounterBufferUAV];
            RWStructuredBuffer<uint2> occlusionCulledMeshletsBuffer = ResourceDescriptorHeap[SceneCB.occlusionCulledMeshletsBufferUAV];

            uint culledMeshlets;
            counterBuffer.InterlockedAdd(c_dispatchIndex * 4, 1, culledMeshlets);

            occlusionCulledMeshletsBuffer[c_occlusionCulledMeshletsBufferOffset + culledMeshlets] = uint2(sceneConstantAddress, meshletIndex);
        }

        stats(STATS_OCCLUSION_CULLED_MESHLET, 1);
        return false;
    }

    return true;
}

[numthreads(32, 1, 1)]
void main_as(uint dispatchThreadID : SV_DispatchThreadID)
{
    bool visible = false;

    uint sceneConstantAddress = 0;
    uint meshletIndex = 0;
    uint totalMeshletCount = 0;

    if (c_bFirstPass)
    {
        uint2 dataPerMeshlet = LoadDataPerMeshlet(dispatchThreadID);
        sceneConstantAddress = dataPerMeshlet.x;
        meshletIndex = dataPerMeshlet.y;

        totalMeshletCount = c_mergedMeshletCount;
    }
    else
    {
        StructuredBuffer<uint2> occlusionCulledMeshletsBuffer = ResourceDescriptorHeap[SceneCB.occlusionCulledMeshletsBufferSRV];
        uint2 dataPerMeshlet = occlusionCulledMeshletsBuffer[c_occlusionCulledMeshletsBufferOffset + dispatchThreadID];
        sceneConstantAddress = dataPerMeshlet.x;
        meshletIndex = dataPerMeshlet.y;
        
        ByteAddressBuffer counterBuffer = ResourceDescriptorHeap[SceneCB.occlusionCulledMeshletsCounterBufferSRV];
        totalMeshletCount = counterBuffer.Load(c_dispatchIndex * 4);
    }
    
    if (dispatchThreadID < totalMeshletCount)
    {
        Meshlet meshlet = LoadSceneBuffer<Meshlet>(GetModelConstant(sceneConstantAddress).meshletBufferAddress, meshletIndex);
        
        visible = Cull(meshlet, meshletIndex, sceneConstantAddress);
        
        stats(visible ? STATS_RENDERED_TRIANGLE : STATS_CULLED_TRIANGLE, meshlet.triangleCount);
    }
    
    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.sceneConstantAddress[index] = sceneConstantAddress;
        s_Payload.meshletIndices[index] = meshletIndex;
    }

    uint visibleMeshletCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleMeshletCount, 1, 1, s_Payload);
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload Payload payload,
    out indices uint3 indices[124],
    out vertices VertexOut vertices[64])
{
    uint sceneConstantAddress = payload.sceneConstantAddress[groupID];
    uint meshletIndex = payload.meshletIndices[groupID];
    if(meshletIndex >= GetModelConstant(sceneConstantAddress).meshletCount)
    {
        return;
    }
    
    Meshlet meshlet = LoadSceneBuffer<Meshlet>(GetModelConstant(sceneConstantAddress).meshletBufferAddress, meshletIndex);
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        uint3 index = uint3(
            LoadSceneBuffer<uint16_t>(GetModelConstant(sceneConstantAddress).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3),
            LoadSceneBuffer<uint16_t>(GetModelConstant(sceneConstantAddress).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 1),
            LoadSceneBuffer<uint16_t>(GetModelConstant(sceneConstantAddress).meshletIndicesBufferAddress, meshlet.triangleOffset + groupThreadID * 3 + 2));

        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {        
        uint vertex_id = LoadSceneBuffer<uint>(GetModelConstant(sceneConstantAddress).meshletVerticesBufferAddress, meshlet.vertexOffset + groupThreadID);

        VertexOut v = GetVertex(sceneConstantAddress, vertex_id);
        v.meshlet = meshletIndex;
        v.sceneConstantAddress = sceneConstantAddress;
        
        vertices[groupThreadID] = v;
    }
}
