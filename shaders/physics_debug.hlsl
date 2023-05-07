#include "common.hlsli"
#include "physics_debug.hlsli"

ConstantBuffer<Constants> PhysicsCB : register(b1);

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<Vertex> vertexBuffer = ResourceDescriptorHeap[PhysicsCB.vertexBuffer];
    Vertex vertex = vertexBuffer[vertex_id];
    
    float4 worldPos = mul(PhysicsCB.mtxWorld, float4(vertex.position, 1.0));

    VSOutput output;
    output.pos = mul(GetCameraCB().mtxViewProjection, worldPos);
    output.uv = vertex.uv;
    output.normal = mul(PhysicsCB.mtxWorldInverseTranspose, float4(vertex.normal, 0.0)).xyz;
    output.color = UnpackRGBA8Unorm(PhysicsCB.color) * UnpackRGBA8Unorm(vertex.color);
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 L = SceneCB.lightDir;
    float lighting = max(0.0, dot(N, L)) + 0.05;
    
    uint2 less_haf = step(frac(input.uv), 0.5);
    float checkerboard = less_haf.x ^ less_haf.y ? 0.5 : 1.0;
    
    return input.color * lighting * checkerboard;
}