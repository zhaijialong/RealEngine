#include "dof_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_colorTexture;
    uint c_depthTexture;
    uint c_farBlurTexture;
    uint c_nearBlurTexture;
    
    uint c_outputTexture;
    float c_focusDistance;
}

static Texture2D colorTexture = ResourceDescriptorHeap[c_colorTexture];
static Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
static Texture2D farBlurTexture = ResourceDescriptorHeap[c_farBlurTexture];
static Texture2D nearBlurTexture = ResourceDescriptorHeap[c_nearBlurTexture];
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 uv = GetScreenUV(dispatchThreadID, SceneCB.rcpRenderSize);
    
    float3 color = colorTexture[dispatchThreadID].xyz;
    float depth = depthTexture[dispatchThreadID].x;
    float4 farBlur = farBlurTexture.SampleLevel(linearSampler, uv, 0);
    float4 nearBlur = nearBlurTexture.SampleLevel(linearSampler, uv, 0);    
    
    const float focalLength = GetCameraCB().physicalCamera.focalLength;
    const float sensorWidth = GetCameraCB().physicalCamera.sensorWidth;
    const float apertureWidth = focalLength / GetCameraCB().physicalCamera.aperture;
    float coc = ComputeCoc(depth, apertureWidth, focalLength, c_focusDistance, sensorWidth);
    
    float farCoc = max(coc, 0.0f);
    float nearCoc = max(-coc, 0.0f);
    
    if(farCoc > 0.0)
    {
        farBlur.xyz /= farCoc;
    }
    
    if(nearCoc > 0.0)
    {
        nearBlur.xyz /= nearCoc;
    }
    
    float farBlend = saturate(farCoc * MAX_COC_SIZE - 0.5);
    color = lerp(color, farBlur.xyz, smoothstep(0.0, 1.0, farBlend));

    float nearBlend = saturate(nearCoc * MAX_COC_SIZE * 2.0);
    color = lerp(color, nearBlur.xyz, smoothstep(0.0, 1.0, nearBlend));
    
    outputTexture[dispatchThreadID] = float4(color, 1.0);
}