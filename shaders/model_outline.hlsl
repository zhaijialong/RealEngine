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
    float4 pos = float4(LoadSceneBuffer<float3>(ModelCB.posBufferAddress, vertex_id), 1.0);
    float3 normal = LoadSceneBuffer<float3>(ModelCB.normalBufferAddress, vertex_id);

    float4 worldPos = mul(ModelCB.mtxWorld, pos);
    float3 worldNormal = mul(ModelCB.mtxWorldInverseTranspose, float4(normal, 0.0)).xyz;
    
    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);

    float3 clipNormal = mul(CameraCB.mtxViewProjection, float4(worldNormal, 0.0)).xyz;

    const float width = 2;
    
    // https://alexanderameye.github.io/notes/rendering-outlines/
    // Move vertex along normal vector in clip space.
    output.pos.xy += normalize(clipNormal.xy) * float2(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight) * output.pos.w * width * 2;

#if ALPHA_TEST
    output.uv = LoadSceneBuffer<float2>(ModelCB.uvBufferAddress, vertex_id);
#endif
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET0
{
#if ALPHA_TEST
    AlphaTest(input.uv);
#endif
    
    return float4(0.6, 0.4, 0, 1);
}