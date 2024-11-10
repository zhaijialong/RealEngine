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
    
    float4 far = float4(color00, 1.0) * max(coc00, 0.0f) +
        float4(color10, 1.0) * max(coc10, 0.0f) +
        float4(color01, 1.0) * max(coc01, 0.0f) +
        float4(color11, 1.0) * max(coc11, 0.0f);
    
    float4 near = float4(color00, max(-coc00, 0.0f)) +
        float4(color10, max(-coc10, 0.0f)) +
        float4(color01, max(-coc01, 0.0f)) +
        float4(color11, max(-coc11, 0.0f));
    
    downsampledFarTexture[dispatchThreadID] = far * 0.25;
    downsampledNearTexture[dispatchThreadID] = near * 0.25;
}