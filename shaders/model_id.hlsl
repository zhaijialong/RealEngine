#include "model.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_ObjectID;
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
    float4 pos = float4(LoadSceneBuffer<float3>(ModelCB.posBufferAddress, vertex_id), 1.0);
    float4 worldPos = mul(ModelCB.mtxWorld, pos);
    
    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);

#if ALPHA_TEST
    output.uv = LoadSceneBuffer<float2>(ModelCB.uvBufferAddress, vertex_id);
#endif
    
    return output;
}

uint ps_main(VSOutput input) : SV_TARGET0
{
#if ALPHA_TEST
    AlphaTest(input.uv);
#endif
    
    return c_ObjectID;
}