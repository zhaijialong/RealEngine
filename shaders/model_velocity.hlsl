#include "model.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
#if ALPHA_TEST
    float2 uv : TEXCOORD;
#endif
    
    float4 clipPos : CLIP_POSITION0;
    float4 prevClipPos : CLIP_POSITION1;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    model::Vertex v = model::GetVertex(c_InstanceIndex, vertex_id);
    InstanceData instanceData = GetInstanceData(c_InstanceIndex);

    float4 pos = float4(v.pos, 1.0);
    float4 worldPos = mul(instanceData.mtxWorld, pos);

    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);
    
#if ALPHA_TEST
    output.uv = v.uv;
#endif

#if ANIME_POS
    float4 prevPos = float4(LoadSceneAnimationBuffer<float3>(c_PrevAnimationPosAddress, vertex_id), 1.0);
#else
    float4 prevPos = pos;
#endif
    float4 prevWorldPos = mul(instanceData.mtxPrevWorld, prevPos);
    
    output.clipPos = mul(CameraCB.mtxViewProjectionNoJitter, worldPos);
    output.prevClipPos = mul(CameraCB.mtxPrevViewProjectionNoJitter, prevWorldPos);
    
    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET0
{    
#if ALPHA_TEST
    model::AlphaTest(c_InstanceIndex, input.uv);
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