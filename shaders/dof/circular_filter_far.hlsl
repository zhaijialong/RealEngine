#include "dof_common.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputRTexture;
    uint c_outputGTexture;
    uint c_outputBTexture;

    uint c_outputTexture;
    uint c_weightsTexture;
    float c_maxCocSize;
}

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float4> outputRTexture = ResourceDescriptorHeap[c_outputRTexture];
static RWTexture2D<float4> outputGTexture = ResourceDescriptorHeap[c_outputGTexture];
static RWTexture2D<float4> outputBTexture = ResourceDescriptorHeap[c_outputBTexture];
static RWTexture2D<float> outputWeightsTexture = ResourceDescriptorHeap[c_weightsTexture];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

[numthreads(8, 8, 1)]
void main_horizontal(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float coc = inputTexture[dispatchThreadID].w;
    if (coc == 0.0)
    {
        outputRTexture[dispatchThreadID] = 0;
        outputGTexture[dispatchThreadID] = 0;
        outputBTexture[dispatchThreadID] = 0;
        outputWeightsTexture[dispatchThreadID] = 0;
        return;
    }
    
    uint2 textureSize = (SceneCB.renderSize.xy + 1) / 2;
    float2 uv = (dispatchThreadID + 0.5) / textureSize;
    float filterRadius = c_maxCocSize * coc * 0.5;
    float2 randomValue = GetVec2STBN(dispatchThreadID, SceneCB.frameIndex) - 0.5;
    
    float4 valR = 0.0;
    float4 valG = 0.0;
    float4 valB = 0.0;
    float weights = 0.0;
    
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        float2 offset = float2((float)i / KERNEL_RADIUS, 0.0);
        float2 coords = uv + (offset * filterRadius + randomValue) / textureSize;
        float4 val = inputTexture.SampleLevel(linearSampler, coords, 0);
        
        float2 c0 = Kernel0_RealX_ImY_RealZ_ImW_2[i + KERNEL_RADIUS].xy;
        float2 c1 = Kernel1_RealX_ImY_RealZ_ImW_2[i + KERNEL_RADIUS].xy;
        
        valR += float4(val.r * c0, val.r * c1);
        valG += float4(val.g * c0, val.g * c1);
        valB += float4(val.b * c0, val.b * c1);
        weights += val.w;
    }

    outputRTexture[dispatchThreadID] = valR;
    outputGTexture[dispatchThreadID] = valG;
    outputBTexture[dispatchThreadID] = valB;
    outputWeightsTexture[dispatchThreadID] = weights / float(KERNEL_COUNT);
}

static Texture2D RTexture = ResourceDescriptorHeap[c_outputRTexture];
static Texture2D GTexture = ResourceDescriptorHeap[c_outputGTexture];
static Texture2D BTexture = ResourceDescriptorHeap[c_outputBTexture];
static Texture2D weightsTexture = ResourceDescriptorHeap[c_weightsTexture];
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

[numthreads(8, 8, 1)]
void main_vertical(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float coc = inputTexture[dispatchThreadID].w;
    if (coc == 0.0)
    {
        outputTexture[dispatchThreadID] = 0;
        return;
    }
    
    uint2 textureSize = (SceneCB.renderSize.xy + 1) / 2;
    float2 uv = (dispatchThreadID + 0.5) / textureSize;
    float filterRadius = c_maxCocSize * coc * 0.5;
    float2 randomValue = GetVec2STBN(dispatchThreadID, SceneCB.frameIndex) - 0.5;
    
    float4 valR = 0.0;
    float4 valG = 0.0;
    float4 valB = 0.0;
    float weights = 0.0;
    
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        float2 offset = float2(0.0, (float) i / KERNEL_RADIUS);
        float2 coords = uv + (offset * filterRadius + randomValue) / textureSize;
        
        float4 R = RTexture.SampleLevel(linearSampler, coords, 0);
        float4 G = GTexture.SampleLevel(linearSampler, coords, 0);
        float4 B = BTexture.SampleLevel(linearSampler, coords, 0);
        float weight = weightsTexture.SampleLevel(linearSampler, coords, 0).x;
        
        float2 c0 = Kernel0_RealX_ImY_RealZ_ImW_2[i + KERNEL_RADIUS].xy;
        float2 c1 = Kernel1_RealX_ImY_RealZ_ImW_2[i + KERNEL_RADIUS].xy;
        
        valR.xy += MulComplex(R.xy, c0);
        valR.zw += MulComplex(R.zw, c1);

        valG.xy += MulComplex(G.xy, c0);
        valG.zw += MulComplex(G.zw, c1);

        valB.xy += MulComplex(B.xy, c0);
        valB.zw += MulComplex(B.zw, c1);
        
        weights += weight;
    }
    
    weights /= float(KERNEL_COUNT);
    
    float redChannel = dot(valR.xy, Kernel0Weights_RealX_ImY_2) + dot(valR.zw, Kernel1Weights_RealX_ImY_2);
    float greenChannel = dot(valG.xy, Kernel0Weights_RealX_ImY_2) + dot(valG.zw, Kernel1Weights_RealX_ImY_2);
    float blueChannel = dot(valB.xy, Kernel0Weights_RealX_ImY_2) + dot(valB.zw, Kernel1Weights_RealX_ImY_2);
    float3 output = 0.0;
    
    if(weights > 0.0)
    {
        output = max(0.0, float3(redChannel, greenChannel, blueChannel)) / weights;
    }
    
    outputTexture[dispatchThreadID] = float4(output, coc);
}