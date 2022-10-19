#include "model.hlsli"

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

    float4 pos = float4(v.pos, 1.0);
    float4 worldPos = mul(GetInstanceData(c_InstanceIndex).mtxWorld, pos);
    
    VSOutput output;
    output.pos = mul(GetCameraCB().mtxViewProjection, worldPos);

#if ALPHA_TEST
    output.uv = v.uv;
#endif
    
    return output;
}

uint ps_main(VSOutput input) : SV_TARGET0
{
#if ALPHA_TEST
    model::AlphaTest(c_InstanceIndex, input.uv);
#endif
    
    return GetInstanceData(c_InstanceIndex).objectID;
}