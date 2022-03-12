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
    
    float3 ndcPos = GetNdcPos(input.clipPos);
    float3 prevNdcPos = GetNdcPos(input.prevClipPos);

    return PackVelocity(ndcPos, prevNdcPos);
}