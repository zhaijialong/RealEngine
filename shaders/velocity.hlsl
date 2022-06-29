#include "common.hlsli"

cbuffer CB : register(b0)
{
    uint c_velocityTexture;
    uint c_depthTexture;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<float2> velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
    float2 velocity = velocityTexture[dispatchThreadID.xy];

    if (any(abs(velocity) > 0.00001))
    {
        return;
    }

    Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
    float depth = depthTexture[dispatchThreadID.xy].x;

    float3 ndcPos = float3(GetNdcPosition((float2)dispatchThreadID.xy + 0.5, SceneCB.rcpRenderSize), depth);
    
    float4 prevClipPos = mul(CameraCB.mtxClipToPrevClipNoJitter, float4(ndcPos, 1.0));
    float3 prevNdcPos = GetNdcPosition(prevClipPos);

    velocityTexture[dispatchThreadID.xy] = ndcPos.xy - prevNdcPos.xy;
}