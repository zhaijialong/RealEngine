#include "common.hlsli"
#include "model_constants.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_meshletBuffer;
    uint c_meshletVerticesBuffer;
    uint c_meshletIndicesBuffer;
};

struct Meshlet
{
    unsigned int vertexOffset;
    unsigned int triangleOffset;

    unsigned int vertexCount;
    unsigned int triangleCount;
};

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
    out indices uint3 tris[124],
    out vertices VertexOut verts[64])
{
    StructuredBuffer<Meshlet> meshletBuffer = ResourceDescriptorHeap[c_meshletBuffer];
    Meshlet meshlet = meshletBuffer[groupID];
    
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);
    
    if(groupThreadID < meshlet.triangleCount)
    {
        StructuredBuffer<uint16_t> meshletIndicesBuffer = ResourceDescriptorHeap[c_meshletIndicesBuffer];
        uint3 tri = uint3(
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 1],
            meshletIndicesBuffer[meshlet.triangleOffset + groupThreadID * 3 + 2]);
        
        tris[groupThreadID] = tri;
    }
    
    if(groupThreadID < meshlet.vertexCount)
    {
        StructuredBuffer<uint> meshletVerticesBuffer = ResourceDescriptorHeap[c_meshletVerticesBuffer];
        uint vertex_id = meshletVerticesBuffer[meshlet.vertexOffset + groupThreadID];
        
        StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
        StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
        
        float4 pos = float4(posBuffer[vertex_id], 1.0);
        float4 worldPos = mul(ModelCB.mtxWorld, pos);
        
        VertexOut v;
        v.pos = mul(CameraCB.mtxViewProjection, worldPos);
        v.normal = normalize(mul(ModelCB.mtxNormal, float4(normalBuffer[vertex_id], 0.0f)).xyz);
        v.meshlet = groupID;
        
        verts[groupThreadID] = v;
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