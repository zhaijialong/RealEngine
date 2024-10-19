#include "dof_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputRTexture;
    uint c_outputGTexture;
    uint c_outputBTexture;
    uint c_outputTexture;
}

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float2> outputRTexture = ResourceDescriptorHeap[c_outputRTexture];
static RWTexture2D<float2> outputGTexture = ResourceDescriptorHeap[c_outputGTexture];
static RWTexture2D<float2> outputBTexture = ResourceDescriptorHeap[c_outputBTexture];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

[numthreads(8, 8, 1)]
void main_horizontal(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float coc = inputTexture[dispatchThreadID].w;
    if(coc == 0.0)
    {
        outputRTexture[dispatchThreadID] = 0;
        outputGTexture[dispatchThreadID] = 0;
        outputBTexture[dispatchThreadID] = 0;
        return;
    }
    
    uint2 textureSize = (SceneCB.renderSize.xy + 1) / 2;
    float2 uv = (dispatchThreadID + 0.5) / textureSize;    
    float filterRadius = MAX_COC_SIZE * coc * 0.5;
    
    float2 valR = 0.0;
    float2 valG = 0.0;
    float2 valB = 0.0;
    
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        float2 offset = float2((float) i / KERNEL_RADIUS, 0.0);
        float2 coords = uv + offset * filterRadius / textureSize;
        float3 val = inputTexture.SampleLevel(linearSampler, coords, 0).xyz;
        
        float2 c0 = Kernel0_RealX_ImY_RealZ_ImW_1[i + KERNEL_RADIUS].xy;
        
        valR += val.r * c0;
        valG += val.g * c0;
        valB += val.b * c0;
    }

    outputRTexture[dispatchThreadID] = valR;
    outputGTexture[dispatchThreadID] = valG;
    outputBTexture[dispatchThreadID] = valB;
}

static Texture2D RTexture = ResourceDescriptorHeap[c_outputRTexture];
static Texture2D GTexture = ResourceDescriptorHeap[c_outputGTexture];
static Texture2D BTexture = ResourceDescriptorHeap[c_outputBTexture];
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
    float filterRadius = MAX_COC_SIZE * coc * 0.5;
    
    float2 valR = 0.0;
    float2 valG = 0.0;
    float2 valB = 0.0;
    
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        float2 offset = float2(0.0, (float) i / KERNEL_RADIUS);
        float2 coords = uv + offset * filterRadius / textureSize;
        
        float2 R = RTexture.SampleLevel(linearSampler, coords, 0).xy;
        float2 G = GTexture.SampleLevel(linearSampler, coords, 0).xy;
        float2 B = BTexture.SampleLevel(linearSampler, coords, 0).xy;
        
        float2 c0 = Kernel0_RealX_ImY_RealZ_ImW_1[i + KERNEL_RADIUS].xy;
        
        valR.xy += MulComplex(R.xy, c0);
        valG.xy += MulComplex(G.xy, c0);
        valB.xy += MulComplex(B.xy, c0);
    }
    
    float redChannel = dot(valR.xy, Kernel0Weights_RealX_ImY_1);
    float greenChannel = dot(valG.xy, Kernel0Weights_RealX_ImY_1);
    float blueChannel = dot(valB.xy, Kernel0Weights_RealX_ImY_1);
    outputTexture[dispatchThreadID] = float4(redChannel, greenChannel, blueChannel, coc);
}