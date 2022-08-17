#include "reservoir.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"

cbuffer CB : register(b1)
{
    uint c_halfDepthNormal;
    uint c_inputReservoirSampleRadiance;
    uint c_inputReservoir;
    uint c_outputReservoirSampleRadiance;
    uint c_outputReservoir;
    uint c_spatialPass;
}

Reservoir LoadReservoir(uint2 pos, float depth, float3 normal)
{    
    Texture2D reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_inputReservoirSampleRadiance];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_inputReservoir];

    Reservoir R;
    R.sample.radiance = reservoirSampleRadianceTexture[pos].xyz;

    R.M = reservoirTexture[pos].x;
    R.W = reservoirTexture[pos].y;
    R.sumWeight = R.W * R.M * TargetFunction(R.sample.radiance);
    
    return R;
}

void StoreReservoir(uint2 pos, Reservoir R)
{
    RWTexture2D<float4> reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_outputReservoirSampleRadiance];
    RWTexture2D<float2> reservoirTexture = ResourceDescriptorHeap[c_outputReservoir];
    
    reservoirSampleRadianceTexture[pos] = float4(R.sample.radiance, 0);
    reservoirTexture[pos] = float2(R.M, R.W);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D<uint2> halfDepthNormalTexture = ResourceDescriptorHeap[c_halfDepthNormal];
    
    float depth = asfloat(halfDepthNormalTexture[pos].x);
    float3 normal = DecodeNormal16x2(halfDepthNormalTexture[pos].y);
    
    float linearDepth = GetLinearDepth(depth);
    
    if (depth == 0.0)
    {
        StoreReservoir(pos, (Reservoir)0);
        return;
    }
    
    uint2 halfScreenSize = (SceneCB.renderSize + 1) / 2;
    PRNG rng = PRNG::Create(pos, halfScreenSize);
    Reservoir Rs = LoadReservoir(pos, depth, normal);
    float3 worldPos = GetWorldPosition(FullScreenPosition(pos), depth);
    
    const uint maxIterations = c_spatialPass == 0 ? 8 : 5;
    const float searchRadius = c_spatialPass == 0 ? 16.0 : 8.0;
    float selected_target_p = TargetFunction(Rs.sample.radiance);
    
    for (uint i = 0; i < maxIterations; ++i)
    {
        uint2 qn = clamp(pos + searchRadius * (rng.RandomFloat2() * 2.0 - 1.0), 0, halfScreenSize);

        float depth_qn = asfloat(halfDepthNormalTexture[qn].x);
        float3 normal_qn = DecodeNormal16x2(halfDepthNormalTexture[qn].y);
        float3 worldPos_qn = GetWorldPosition(FullScreenPosition(qn), depth_qn);
        
        if (length(worldPos - worldPos_qn) > 0.05 * linearDepth ||
            saturate(dot(normal, normal_qn)) < 0.9)
        {
            continue;
        }
        
        Reservoir Rn = LoadReservoir(qn, depth_qn, normal_qn);
        
        // just ignore jacobian for performance ...
        float target_p = TargetFunction(Rn.sample.radiance);// / jacobian; 

        // and ignore visibility for performance ...
        //if Rn's sample point is not visible to xv at q , target_p = 0
        
        if(Rs.Merge(Rn, target_p, rng.RandomFloat()))
        {
            selected_target_p = target_p;
        }
    }

    Rs.W = Rs.sumWeight / max(0.00001, Rs.M * selected_target_p);
    //Rs.W = min(Rs.W, 10.0);
    
    StoreReservoir(pos, Rs);
}