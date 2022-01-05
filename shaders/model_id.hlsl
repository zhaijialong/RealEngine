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
    float4 pos = float4(LoadSceneBuffer<float3>(GetModelConstant().posBufferAddress, vertex_id), 1.0);
    float4 worldPos = mul(GetModelConstant().mtxWorld, pos);
    
    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);

#if ALPHA_TEST
    output.uv = LoadSceneBuffer<float2>(GetModelConstant().uvBufferAddress, vertex_id);
#endif
    
    return output;
}

uint ps_main(VSOutput input) : SV_TARGET0
{
#if ALPHA_TEST
    AlphaTest(input.uv);
#endif
    
    return GetModelConstant().objectID;
}