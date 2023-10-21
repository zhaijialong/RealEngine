#include "../common.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b0)
{
    uint c_colorTexture;
    uint c_depthTexture;
    uint c_velocityTexture;
    uint c_neighborMaxTexture;

    uint c_outputTexture;
    uint c_tileSize;
    uint c_sampleCount;
};

static Texture2D colorTexture = ResourceDescriptorHeap[c_colorTexture];
static Texture2D depthTexture = ResourceDescriptorHeap[c_depthTexture];
static Texture2D velocityTexture = ResourceDescriptorHeap[c_velocityTexture];
static Texture2D neighborMaxTexture = ResourceDescriptorHeap[c_neighborMaxTexture];
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
static SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

float GetVelocityLength(float2 v)
{
    const float exposureTime = GetCameraCB().physicalCamera.shutterSpeed;
    const float frameRate = 1.0 / SceneCB.frameTime;
    
    return length(v * 0.5 * float2(SceneCB.displaySize) * exposureTime * frameRate);
}

float cone(float dist, float velocityLength)
{
    return saturate(1.0 - dist / velocityLength);
}

float cylinder(float dist, float velocityLength)
{
    return 1.0 - smoothstep(0.95 * velocityLength, 1.05 * velocityLength, dist);
}

float softDepthCompare(float za, float zb)
{
    const float SOFT_Z_EXTENT = 0.1;
    return saturate(1.0 - (za - zb) / SOFT_Z_EXTENT);
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 maxNeighborVelocity = neighborMaxTexture[dispatchThreadID / c_tileSize].xy;
    float3 color = colorTexture[dispatchThreadID].xyz;
    
    if (GetVelocityLength(maxNeighborVelocity) <= 0.5) //convert to texture space
    {
        outputTexture[dispatchThreadID] = float4(color, 1.0);
        return;
    }
    
    float2 uv = GetScreenUV(dispatchThreadID.xy, SceneCB.rcpDisplaySize);
    float2 velocity = velocityTexture.SampleLevel(pointSampler, uv, 0.0).xy;
    float velocityLength = GetVelocityLength(velocity);
    float depth = GetLinearDepth(depthTexture.SampleLevel(pointSampler, uv, 0.0).x);
    
    float weight = 1.0 / max(0.000001, velocityLength);
    float3 sum = color * weight;
    
    PRNG rng;
    rng.seed = PCG(dispatchThreadID.x + dispatchThreadID.y * SceneCB.displaySize.x);
    float random = rng.RandomFloat();
    
    for (uint i = 0; i < c_sampleCount; ++i)
    {
        if (i == (c_sampleCount - 1) / 2)
        {
            continue;
        }
        
        float t = lerp(-1.0, 1.0, (i + random + 1.0) / (c_sampleCount + 1.0));
        float2 uvY = uv + t * maxNeighborVelocity * float2(0.5, -0.5);
        float zY = GetLinearDepth(depthTexture.SampleLevel(pointSampler, uvY, 0.0).x);
        float2 vY = velocityTexture.SampleLevel(pointSampler, uvY, 0.0).xy;
        
        float dist = length((uv - uvY) * SceneCB.displaySize);
        float velocityLengthY = GetVelocityLength(vY);
        
        float f = softDepthCompare(depth, zY);
        float b = softDepthCompare(zY, depth);
        
        float aY = f * cone(dist, velocityLengthY);
        aY += b * cone(dist, velocityLength);
        aY += cylinder(dist, velocityLengthY) * cylinder(dist, velocityLength) * 2.0;
        
        weight += aY;
        sum += aY * colorTexture.SampleLevel(linearSampler, uvY, 0.0).xyz;
    }
    
    outputTexture[dispatchThreadID] = float4(sum / weight, 1);
}