#include "dof_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_colorTexture;
    uint c_depthTexture;
    uint c_downsampledFarTexture;
    uint c_downsampledNearTexture;
    
    float c_focusDistance;
    float c_maxCocSize;
}

static Texture2D colorTexture = ResourceDescriptorHeap[c_colorTexture];
static Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
static RWTexture2D<float4> downsampledFarTexture = ResourceDescriptorHeap[c_downsampledFarTexture];
static RWTexture2D<float4> downsampledNearTexture = ResourceDescriptorHeap[c_downsampledNearTexture];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    const float focalLength = GetCameraCB().physicalCamera.focalLength;
    const float sensorWidth = GetCameraCB().physicalCamera.sensorWidth;
    const float apertureWidth = focalLength / GetCameraCB().physicalCamera.aperture;
    
    float3 color00 = colorTexture[dispatchThreadID * 2 + uint2(0, 0)].xyz;
    float3 color10 = colorTexture[dispatchThreadID * 2 + uint2(1, 0)].xyz;
    float3 color01 = colorTexture[dispatchThreadID * 2 + uint2(0, 1)].xyz;
    float3 color11 = colorTexture[dispatchThreadID * 2 + uint2(1, 1)].xyz;
    
    float depth00 = depthTexture[dispatchThreadID * 2 + uint2(0, 0)].x;
    float depth10 = depthTexture[dispatchThreadID * 2 + uint2(1, 0)].x;
    float depth01 = depthTexture[dispatchThreadID * 2 + uint2(0, 1)].x;
    float depth11 = depthTexture[dispatchThreadID * 2 + uint2(1, 1)].x;
    
    float coc00 = ComputeCoc(depth00, apertureWidth, focalLength, c_focusDistance, sensorWidth, c_maxCocSize);
    float coc10 = ComputeCoc(depth10, apertureWidth, focalLength, c_focusDistance, sensorWidth, c_maxCocSize);
    float coc01 = ComputeCoc(depth01, apertureWidth, focalLength, c_focusDistance, sensorWidth, c_maxCocSize);
    float coc11 = ComputeCoc(depth11, apertureWidth, focalLength, c_focusDistance, sensorWidth, c_maxCocSize);
    
    float farCoc00 = max(coc00, 0.0f);
    float farCoc10 = max(coc10, 0.0f);
    float farCoc01 = max(coc01, 0.0f);
    float farCoc11 = max(coc11, 0.0f);
    
    float w00 = rcp(4.0 + Luminance(color00));
    float w10 = rcp(4.0 + Luminance(color10));
    float w01 = rcp(4.0 + Luminance(color01));
    float w11 = rcp(4.0 + Luminance(color11));
    
    float4 far;
    far.xyz = (color00 * farCoc00 * w00 + color10 * farCoc10 * w10 + color01 * farCoc01 * w01 + color11 * farCoc11 * w11) / (w00 + w10 + w01 + w11);
    far.w = max(max(farCoc00, farCoc01), max(farCoc10, farCoc11));
    
    float nearCoc00 = max(-coc00, 0.0f);
    float nearCoc10 = max(-coc10, 0.0f);
    float nearCoc01 = max(-coc01, 0.0f);
    float nearCoc11 = max(-coc11, 0.0f);
    
    float4 near;
    near.xyz = (color00 * nearCoc00 * w00 + color10 * nearCoc10 * w10 + color01 * nearCoc01 * w01 + color11 * nearCoc11 * w11) / (w00 + w10 + w01 + w11);
    near.w = max(max(nearCoc00, nearCoc01), max(nearCoc10, nearCoc11));
    
    downsampledFarTexture[dispatchThreadID] = far;
    downsampledNearTexture[dispatchThreadID] = near;
}