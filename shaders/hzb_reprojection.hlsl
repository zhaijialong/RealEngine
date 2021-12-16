#include "common.hlsli"

cbuffer ReprojectionConsts : register(b0)
{
    uint c_prevLinearDepthSRV;
    uint c_reprojectedDepthUAV;
    uint c_depthWidth;
    uint c_depthHeight;
};

[numthreads(8, 8, 1)]
void depth_reprojection(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> prevLinearDepthTexture = ResourceDescriptorHeap[c_prevLinearDepthSRV];
    RWTexture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_reprojectedDepthUAV];
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.maxReductionSampler];
    
    //clear
    reprojectedDepthTexture[dispatchThreadID.xy] = 0.0f;
    
    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_depthWidth, c_depthHeight);
    
    float prevLinearDepth = prevLinearDepthTexture.Sample(maxReductionSampler, uv);
    float prevNdcDepth = GetNdcDepth(prevLinearDepth);
    
    float4 clipPos = float4((uv * 2.0 - 1.0) * float2(1.0, -1.0), prevNdcDepth, 1.0);
    float4 worldPos = mul(CameraCB.mtxPrevViewProjectionInverse, clipPos);
    worldPos /= worldPos.w;
    
    float4 reprojectedPosition = mul(CameraCB.mtxViewProjection, worldPos);
    reprojectedPosition /= reprojectedPosition.w;
    
    float reprojectedDepth = reprojectedPosition.w < 0.0f ? prevNdcDepth : reprojectedPosition.z;
    
    float2 reprojectedUV = GetScreenUV(reprojectedPosition.xy);
    float2 reprojectedScreenPos = reprojectedUV * float2(c_depthWidth, c_depthHeight);
    
    reprojectedDepthTexture[reprojectedScreenPos] = reprojectedDepth;
}

cbuffer DilationConsts : register(b0)
{
    uint c_reprojectedDepthSRV;
    uint c_hzbMip0UAV;
    uint c_hzbWidth;
    uint c_hzbHeight;
};

[numthreads(8, 8, 1)]
void depth_dilation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_reprojectedDepthSRV];
    RWTexture2D<float> hzbMip0UAV = ResourceDescriptorHeap[c_hzbMip0UAV];
    
    float depth = reprojectedDepthTexture[dispatchThreadID.xy];
    
    if(depth == 0.0)
    {
        const int2 offsets[8] = { int2(-1, -1), int2(-1, 0), int2(-1, 1), int2(0, -1), int2(0, 1), int2(1, -1), int2(1, 0), int2(1, 1) };
        
        float minDepth = 1.0f;
        for (int i = 0; i < 8; ++i)
        {
            float d = reprojectedDepthTexture[dispatchThreadID.xy + offsets[i]];
            if(d > 0.0 && d < minDepth)
            {
                minDepth = d;
            }
        }
        
        if(minDepth != 1.0f)
        {
            depth = minDepth;
        }
    }
    
    hzbMip0UAV[dispatchThreadID.xy] = depth;
}