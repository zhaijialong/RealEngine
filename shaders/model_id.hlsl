#include "common.hlsli"
#include "model_constants.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_ObjectID;
};

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
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    float4 worldPos = mul(ModelCB.mtxWorld, pos);
    
    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);

#if ALBEDO_TEXTURE && ALPHA_TEST
    output.uv = uvBuffer[vertex_id];
#endif
    
    return output;
}

uint ps_main(VSOutput input) : SV_TARGET0
{
#if ALBEDO_TEXTURE && ALPHA_TEST
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearRepeatSampler];
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    
    clip(albedo.a - MaterialCB.alphaCutoff);
#endif
    
    return c_ObjectID;
}