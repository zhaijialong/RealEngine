#include "common.hlsli"

cbuffer CB : register(b1)
{
    uint c_posBuffer;
    uint c_prevPosBuffer;
    uint c_uvBuffer;
    uint c_albedoTexture;
    
    float c_alphaCutoff;
    float3 _padding;

    float4x4 c_mtxWVP;
    float4x4 c_mtxWVPNoJitter;
    float4x4 c_mtxPrevWVPNoJitter;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    
    float4 clipPos : CLIP_POSITION0;
    float4 prevClipPos : CLIP_POSITION1;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[c_posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[c_uvBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);

    VSOutput output;
    output.pos = mul(c_mtxWVP, pos);
    output.uv = uvBuffer[vertex_id];
    
    float4 prevPos = pos;
#if ANIME_POS
    StructuredBuffer<float3> prevPosBuffer = ResourceDescriptorHeap[c_prevPosBuffer];
    prevPos = float4(prevPosBuffer[vertex_id], 1.0);
#endif
    
    output.clipPos = mul(c_mtxWVPNoJitter, pos);
    output.prevClipPos = mul(c_mtxPrevWVPNoJitter, prevPos);
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET0
{    
#if ALBEDO_TEXTURE && ALPHA_TEST
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearRepeatSampler];
    Texture2D albedoTexture = ResourceDescriptorHeap[c_albedoTexture];
	float4 albedo = albedoTexture.Sample(linearSampler, input.uv);
    
    clip(albedo.a - c_alphaCutoff);
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