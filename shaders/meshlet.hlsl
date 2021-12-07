#include "common.hlsli"
#include "model_constants.hlsli"
#include "debug.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_meshletCount;
    uint c_meshletBoundsBuffer;
    uint c_meshletBuffer;
    uint c_meshletVerticesBuffer;
    uint c_meshletIndicesBuffer;
};

struct Meshlet
{
    uint vertexOffset;
    uint triangleOffset;

    uint vertexCount;
    uint triangleCount;
};

struct MeshletBound
{
    float3 center;
    float radius;
    
    uint cone; //axis + cutoff, rgba8snorm
};

struct Payload
{
    uint meshletIndices[32];
};

groupshared Payload s_Payload;

bool Cull(MeshletBound meshletBound)
{
    // 1. frustum culling
    float3 center = mul(ModelCB.mtxWorld, float4(meshletBound.center, 1.0)).xyz;
    float radius = meshletBound.radius * ModelCB.scale;
    
    for (uint i = 0; i < 6; ++i)
    {
        if (dot(center, CameraCB.culling.planes[i].xyz) + CameraCB.culling.planes[i].w + radius < 0)
        {
            return false;
        }
    }
    
    // 2. backface culling
    int16_t4 cone = unpack_s8s16((int8_t4_packed)meshletBound.cone);
    float3 axis = cone.xyz / 127.0;
    float cutoff = cone.w / 127.0;
    
    axis = normalize(mul(ModelCB.mtxWorld, float4(axis, 0.0)).xyz);
    float3 view = center - CameraCB.culling.viewPos;
    
    if (dot(view, -axis) >= cutoff * length(view) + radius)
    {
        //DrawDebugSphere(center, radius, float3(1, 0, 0));
        return false;
    }
    
    // 3. occlusion culling(todo)
    
    //DrawDebugSphere(center, radius, float3(0, 1, 0));
    return true;
}

[numthreads(32, 1, 1)]
void main_as(uint dispatchThreadID : SV_DispatchThreadID)
{
    bool visible = false;
    uint meshletIndex = dispatchThreadID;
    
    if (meshletIndex < c_meshletCount)
    {
        StructuredBuffer<MeshletBound> meshletBoundBuffer = ResourceDescriptorHeap[c_meshletBoundsBuffer];
        MeshletBound meshletBound = meshletBoundBuffer[meshletIndex];
        
        visible = Cull(meshletBound);
    }
    
    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.meshletIndices[index] = meshletIndex;
    }

    uint visibleMeshletCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleMeshletCount, 1, 1, s_Payload);
}

struct VertexOut
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    uint meshlet : COLOR;
};

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload Payload payload,
    out indices uint3 indices[124],
    out vertices VertexOut vertices[64])
{
    uint meshletIndex = payload.meshletIndices[groupID];
    if(meshletIndex >= c_meshletCount)
    {
        return;
    }
    
    StructuredBuffer<Meshlet> meshletBuffer = ResourceDescriptorHeap[c_meshletBuffer];
    Meshlet meshlet = meshletBuffer[meshletIndex];
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        StructuredBuffer<uint16_t> meshletIndicesBuffer = ResourceDescriptorHeap[c_meshletIndicesBuffer];
        uint3 index = uint3(
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 1],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 2]);
        
        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {
        StructuredBuffer<uint> meshletVerticesBuffer = ResourceDescriptorHeap[c_meshletVerticesBuffer];
        uint vertex_id = meshletVerticesBuffer[meshlet.vertexOffset + groupThreadID];
        
        StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
        StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
        
        float4 pos = float4(posBuffer[vertex_id], 1.0);
        float4 worldPos = mul(ModelCB.mtxWorld, pos);
        
        VertexOut vertex;
        vertex.pos = mul(CameraCB.mtxViewProjection, worldPos);
        vertex.normal = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(normalBuffer[vertex_id], 0.0f)).xyz);
        vertex.meshlet = meshletIndex;
        
        vertices[groupThreadID] = vertex;
    }
}


uint hash(uint a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

struct GBufferOutput
{
    float4 albedoRT : SV_TARGET0;
    float4 normalRT : SV_TARGET1;
    float4 emissiveRT : SV_TARGET2;
};

GBufferOutput main_ps(VertexOut input)
{
    uint mhash = hash(input.meshlet);
    float3 color = float3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
    
    GBufferOutput output;
    output.albedoRT = float4(color, 0);
    output.normalRT = float4(OctNormalEncode(input.normal), 1);
    output.emissiveRT = float4(0, 0, 0, 1);
    
    return output;
}