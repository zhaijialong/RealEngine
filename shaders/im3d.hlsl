#include "common.hlsli"

// Im3d::VertexData
struct Vertex
{
    float4 positionSize;
    uint color;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 UV : TEXCOORD;
    noperspective float size : SIZE;
    noperspective float edgeDistance : EDGE_DISTANCE;
};

VertexOutput vs_main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VertexOutput output = (VertexOutput)0;

    return output;
}

float4 ps_main(VertexOutput input) : SV_Target
{
    return 0;
}