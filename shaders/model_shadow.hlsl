#include "model.hlsli"

cbuffer ProjectionCB : register(b3)
{
    float4x4 c_mtxShadowViewProjection;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
#if ALPHA_TEST
    float2 uv : TEXCOORD;
#endif
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    model::Vertex v = model::GetVertex(c_InstanceIndex, vertex_id);
    InstanceData instanceData = GetInstanceData(c_InstanceIndex);

    float4 pos = float4(v.pos, 1.0);
    float4 worldPos = mul(instanceData.mtxWorld, pos);
    
    VSOutput output;
    output.pos = mul(c_mtxShadowViewProjection, worldPos);

#if ALPHA_TEST
    output.uv = v.uv;
#endif
    
    return output;
}

void ps_main(VSOutput input)
{
#if ALPHA_TEST
    model::AlphaTest(c_InstanceIndex, input.uv);
#endif
}