#include "common.hlsli"
#include "model_constants.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
#if ALBEDO_TEXTURE && ALPHA_TEST
    float2 uv : TEXCOORD;
#endif
    
    float4 clipPos : CLIP_POSITION0;
    float4 prevClipPos : CLIP_POSITION1;
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

#if ANIME_POS
    StructuredBuffer<float3> prevPosBuffer = ResourceDescriptorHeap[ModelCB.prevPosBuffer];
    float4 prevPos = float4(prevPosBuffer[vertex_id], 1.0);
#else
    float4 prevPos = pos;
#endif
    float4 prevWorldPos = mul(ModelCB.mtxPrevWorld, prevPos);
    
    output.clipPos = mul(CameraCB.mtxViewProjectionNoJitter, worldPos);
    output.prevClipPos = mul(CameraCB.mtxPrevViewProjectionNoJitter, prevWorldPos);
    
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
    
    float2 ndcPos = input.clipPos.xy / input.clipPos.w;
    float2 screenPos = GetScreenPosition(ndcPos);
    
    float2 prevNdcPos = input.prevClipPos.xy / input.prevClipPos.w;
    float2 prevScreenPos = GetScreenPosition(prevNdcPos);
    
    float2 motion = prevScreenPos.xy - screenPos.xy;
    
    float linearZ = input.clipPos.w;
    float prevLinearZ = input.prevClipPos.w;    
    float deltaZ = prevLinearZ - linearZ;
    
    return float4(motion, deltaZ, 0);
}