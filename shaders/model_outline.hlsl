#include "common.hlsli"
#include "model_constants.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
#if ALBEDO_TEXTURE && ALPHA_TEST
    float2 uv : TEXCOORD;
#endif
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    float3 normal = normalBuffer[vertex_id];

    float4 worldPos = mul(ModelCB.mtxWorld, pos);
    float3 worldNormal = mul(ModelCB.mtxNormal, float4(normal, 0.0)).xyz;
    
    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);

    float3 clipNormal = mul(CameraCB.mtxViewProjection, float4(worldNormal, 0.0)).xyz;

    const float width = 2;
    
    // https://alexanderameye.github.io/notes/rendering-outlines/
    // Move vertex along normal vector in clip space.
    output.pos.xy += normalize(clipNormal.xy) * float2(SceneCB.rcpViewWidth, SceneCB.rcpViewHeight) * output.pos.w * width * 2;

#if ALBEDO_TEXTURE && ALPHA_TEST
    output.uv = uvBuffer[vertex_id];
#endif
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET0
{
#if ALBEDO_TEXTURE && ALPHA_TEST
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearRepeatSampler];
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    
    clip(albedo.a - MaterialCB.alphaCutoff);
#endif
    
    return float4(0.6, 0.4, 0, 1);
}