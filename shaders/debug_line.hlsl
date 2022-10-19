#include "debug.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_debugLineVertexBufferSRV;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<debug::LineVertex> vertexBuffer = ResourceDescriptorHeap[c_debugLineVertexBufferSRV];
    
    VSOutput output;
    output.position = mul(GetCameraCB().mtxViewProjectionNoJitter, float4(vertexBuffer[vertex_id].position, 1.0));
    output.color = UnpackRGBA8Unorm(vertexBuffer[vertex_id].color);

    return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
    return input.color;
}