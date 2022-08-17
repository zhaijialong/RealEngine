#include "reservoir.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b1)
{
    uint c_halfDepthNormal;
    uint c_velocity;
    uint c_candidateRadiance;
    uint c_historyReservoirDepthNormal;
    uint c_historyReservoirSampleRadiance;
    uint c_historyReservoir;
    uint c_outputReservoirDepthNormal;
    uint c_outputReservoirSampleRadiance;
    uint c_outputReservoir;
}

Sample LoadInitialSample(uint2 pos)
{
    Texture2D candidateRadianceTexture = ResourceDescriptorHeap[c_candidateRadiance];
    
    Sample S;
    S.radiance = candidateRadianceTexture[pos].xyz;
    
    return S;
}

Reservoir LoadTemporalReservoir(uint2 pos)
{
    Texture2D reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_historyReservoirSampleRadiance];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_historyReservoir];

    Reservoir R;
    R.sample.radiance = reservoirSampleRadianceTexture[pos].xyz;

    R.M = reservoirTexture[pos].x;
    R.W = reservoirTexture[pos].y;
    R.sumWeight = R.W * R.M * TargetFunction(R.sample.radiance);
    
    return R;
}

void StoreTemporalReservoir(uint2 pos, Reservoir R, float depth, float3 normal)
{
    RWTexture2D<uint2> reservoirDepthNormalTexture = ResourceDescriptorHeap[c_outputReservoirDepthNormal];
    RWTexture2D<float4> reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_outputReservoirSampleRadiance];
    RWTexture2D<float2> reservoirTexture = ResourceDescriptorHeap[c_outputReservoir];
    
    reservoirDepthNormalTexture[pos] = uint2(asuint(depth), EncodeNormal16x2(normal));
    reservoirSampleRadianceTexture[pos] = float4(R.sample.radiance, 0);
    reservoirTexture[pos] = float2(R.M, R.W);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    uint2 fullscreenPos = FullScreenPosition(pos);
    
    Texture2D<uint2> halfDepthNormalTexture = ResourceDescriptorHeap[c_halfDepthNormal];
    
    float depth = asfloat(halfDepthNormalTexture[pos].x);
    float3 normal = DecodeNormal16x2(halfDepthNormalTexture[pos].y);
    
    if (depth == 0.0)
    {
        StoreTemporalReservoir(pos, (Reservoir)0, depth, normal);
        return;
    }
    
    uint2 halfScreenSize = (SceneCB.renderSize + 1) / 2;
    PRNG rng = PRNG::Create(pos, halfScreenSize);
    Sample S = LoadInitialSample(pos);
    float3 worldPos = GetWorldPosition(fullscreenPos, depth);
    
    Reservoir R = (Reservoir)0;
    
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocity];
    float2 velocity = velocityTexture[fullscreenPos].xy;
    float2 prevUV = GetScreenUV(fullscreenPos, SceneCB.rcpRenderSize) - velocity * float2(0.5, -0.5);
    bool outOfBound = any(prevUV < 0.0) || any(prevUV > 1.0);
    
    if (!outOfBound)
    {
        uint2 prevPos = (uint2)floor(prevUV * SceneCB.renderSize);
        uint2 prevHalfPos = (uint2)floor(prevUV * halfScreenSize);
        R = LoadTemporalReservoir(prevHalfPos);
        
        Texture2D<uint2> historyReservoirDepthNormal = ResourceDescriptorHeap[c_historyReservoirDepthNormal];
        float prevDepth = asfloat(historyReservoirDepthNormal[prevHalfPos].x);
        float3 prevNormal = DecodeNormal16x2(historyReservoirDepthNormal[prevHalfPos].y);
        
        float4 clipPos = float4((prevUV * 2.0 - 1.0) * float2(1.0, -1.0), prevDepth, 1.0);
        float4 prevWorldPos = mul(CameraCB.mtxPrevViewProjectionInverse, clipPos);
        prevWorldPos.xyz /= prevWorldPos.w;
        
        bool invalid = false;
        invalid |= length(worldPos - prevWorldPos.xyz) > 0.1 * GetLinearDepth(depth);
        invalid |= saturate(dot(normal, prevNormal)) < 0.8;
        
        if (invalid)
        {
            R.sumWeight = R.M = 0;
        }
    }
    
    float target_p_q = TargetFunction(S.radiance);
    float p_q = 1.0 / (2.0 * M_PI);
    float w = target_p_q /*/ p_q*/;

    R.Update(S, w, rng.RandomFloat());
    R.W = R.sumWeight / max(0.00001, R.M * TargetFunction(R.sample.radiance));
    R.M = min(R.M, 30.0);
    
    StoreTemporalReservoir(pos, R, depth, normal);
}
