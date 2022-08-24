#include "blur_common.hlsli"
#include "sh.hlsli"
#include "../random.hlsli"
#include "../debug.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputSHTexture;
    uint c_varianceTexture;
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_outputSHTexture;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint4> inputSHTexture = ResourceDescriptorHeap[c_inputSHTexture];
    Texture2D<float> varianceTexture = ResourceDescriptorHeap[c_varianceTexture];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<uint4> outputSHTexture = ResourceDescriptorHeap[c_outputSHTexture];

    float depth = depthTexture[pos];
    if (depth == 0.0)
    {
        outputSHTexture[pos] = 0;
        return;
    }
    
    float3 worldPos = GetWorldPosition(pos, depth);
    float3 N = DecodeNormal(normalTexture[pos].xyz);
    
    float variance = varianceTexture[pos];
    const float2 blurRadius = lerp(2.0, 8.0, variance) * SceneCB.rcpRenderSize * GetLinearDepth(depth);
    
    SH sh = UnpackSH(inputSHTexture[pos]);
    float sumWeight = 1.0;
    
    [unroll]
    for (uint i = 0; i < SAMPLE_NUM; ++i)
    {
        float2 random = POISSON_SAMPLES[i];
        float2 uv = GetSampleUV(worldPos, N, random * blurRadius);
        
        if (any(uv < 0.0) || any(uv > 1.0))
        {
            continue;
        }

        uint2 samplePos = uint2(uv * SceneCB.renderSize);
        float sampleDepth = depthTexture[samplePos];
        float3 sampleWorldPos = GetWorldPosition(samplePos, sampleDepth);
        float3 sampleNormal = DecodeNormal(normalTexture[samplePos].xyz);

        float geometryWeight = GetGeometryWeight(worldPos, N, sampleWorldPos, 1.0);
        float normalWeight = GetNormalWeight(N, sampleNormal);
        float weight = smoothstep(0, 1, geometryWeight * normalWeight);
        
        sh = sh + UnpackSH(inputSHTexture[samplePos]) * weight;
        sumWeight += weight;
        
        if (all(pos == SceneCB.renderSize / 2))
        {
            //debug::DrawSphere(sampleWorldPos, 0.001, float3(1, 0, 0));
        }
    }
    
    sh = sh * rcp(sumWeight);
    
    outputSHTexture[pos] = PackSH(sh);
}