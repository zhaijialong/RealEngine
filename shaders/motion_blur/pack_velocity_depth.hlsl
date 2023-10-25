#include "motion_blur_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_velocityTexture;
    uint c_depthTexture;
    uint c_outputTexture;
};

static Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
static Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
static RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 uv = GetScreenUV(dispatchThreadID, SceneCB.rcpDisplaySize);
    
    const float exposureTime = GetCameraCB().physicalCamera.shutterSpeed;
    const float frameRate = 1.0 / SceneCB.frameTime;
    
    float2 velocity = velocityTexture.SampleLevel(pointSampler, uv, 0).xy;
    velocity = EncodeVelocity(velocity * exposureTime * frameRate);
    
    float depth = clamp(GetLinearDepth(depthTexture.SampleLevel(pointSampler, uv, 0).x), 0.0, 65000.0);
    
    outputTexture[dispatchThreadID] = float3(velocity, depth);
}