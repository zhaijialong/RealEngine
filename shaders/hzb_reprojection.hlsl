#include "common.hlsli"

cbuffer RootConsts : register(b0)
{
    uint c_inputSRV;
    uint c_outputUAV;
    uint c_hzbWidth;
    uint c_hzbHeight;
};

[numthreads(8, 8, 1)]
void depth_reprojection(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> prevDepthTexture = ResourceDescriptorHeap[SceneCB.prevSceneDepthSRV];
    RWTexture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_outputUAV];
    
    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_hzbWidth, c_hzbHeight);

#if SUPPORTS_MIN_MAX_FILTER
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.maxReductionSampler];
    float prevNdcDepth = prevDepthTexture.SampleLevel(maxReductionSampler, uv, 0);
#else
    SamplerState pointClampSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    float4 prevNdcDepth4 = prevDepthTexture.GatherRed(pointClampSampler, uv);
    float prevNdcDepth = min(min(prevNdcDepth4.x, prevNdcDepth4.y), min(prevNdcDepth4.z, prevNdcDepth4.w));
#endif
    
    float4 clipPos = float4((uv * 2.0 - 1.0) * float2(1.0, -1.0), prevNdcDepth, 1.0);
    float4 worldPos = mul(GetCameraCB().mtxPrevViewProjectionInverse, clipPos);
    worldPos /= worldPos.w;
    
    float4 reprojectedPosition = mul(GetCameraCB().mtxViewProjection, worldPos);
    reprojectedPosition /= reprojectedPosition.w;
    
    float reprojectedDepth = reprojectedPosition.w < 0.0f ? prevNdcDepth : reprojectedPosition.z;
    
    float2 reprojectedUV = GetScreenUV(reprojectedPosition.xy);
    float2 reprojectedScreenPos = reprojectedUV * float2(c_hzbWidth, c_hzbHeight);
    
    reprojectedDepthTexture[reprojectedScreenPos] = saturate(reprojectedDepth);
}

[numthreads(8, 8, 1)]
void depth_dilation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float> hzbMip0UAV = ResourceDescriptorHeap[c_outputUAV];
    
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

[numthreads(8, 8, 1)]
void init_hzb(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> inputDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float> ouputDepthTexture = ResourceDescriptorHeap[c_outputUAV];
    
    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_hzbWidth, c_hzbHeight);

#if SUPPORTS_MIN_MAX_FILTER
    SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.minReductionSampler];
    float minDepth = inputDepthTexture.SampleLevel(minReductionSampler, uv, 0);
#else
    SamplerState pointClampSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    float4 depth = inputDepthTexture.GatherRed(pointClampSampler, uv);
    float minDepth = min(min(depth.x, depth.y), min(depth.z, depth.w));
#endif
    
    ouputDepthTexture[dispatchThreadID.xy] = minDepth;
}

[numthreads(8, 8, 1)]
void init_scene_hzb(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> inputDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float2> ouputDepthTexture = ResourceDescriptorHeap[c_outputUAV];

    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_hzbWidth, c_hzbHeight);

#if SUPPORTS_MIN_MAX_FILTER
    SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.minReductionSampler];
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.maxReductionSampler];
    float minDepth = inputDepthTexture.SampleLevel(minReductionSampler, uv, 0);
    float maxDepth = inputDepthTexture.SampleLevel(maxReductionSampler, uv, 0);
#else
    SamplerState pointClampSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    float4 depth = inputDepthTexture.GatherRed(pointClampSampler, uv);
    float minDepth = min(min(depth.x, depth.y), min(depth.z, depth.w));
    float maxDepth = max(max(depth.x, depth.y), max(depth.z, depth.w));
#endif
    
    ouputDepthTexture[dispatchThreadID.xy] = float2(minDepth, maxDepth);
}
