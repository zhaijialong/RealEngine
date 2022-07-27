#include "reservoir.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b1)
{
    uint c_depth;
    uint c_normal;
    uint c_velocity;
    uint c_candidateIrradiance;
    uint c_candidateHitNormal;
    uint c_candidateRay;
    uint c_historyReservoirPosition;
    uint c_historyReservoirNormal;
    uint c_historyReservoirRayDirection;
    uint c_historyReservoirSampleNormal;
    uint c_historyReservoirSampleRadiance;
    uint c_historyReservoir;
    uint c_outputReservoirPosition;
    uint c_outputReservoirNormal;
    uint c_outputReservoirRayDirection;
    uint c_outputReservoirSampleNormal;
    uint c_outputReservoirSampleRadiance;
    uint c_outputReservoir;
}

Sample LoadInitialSample(uint2 pos, float depth)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normal];
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocity];
    Texture2D candidateIrradianceTexture = ResourceDescriptorHeap[c_candidateIrradiance];
    Texture2D candidateHitNormalTexture = ResourceDescriptorHeap[c_candidateHitNormal];
    Texture2D candidateRayTexture = ResourceDescriptorHeap[c_candidateRay];
    
    float3 candidateRay = OctDecode(candidateRayTexture[pos].xy * 2.0 - 1.0);
    float candidateRayT = candidateIrradianceTexture[pos].w;
    
    Sample S;
    S.x_v = GetWorldPosition(pos, depth);
    S.n_v = DecodeNormal(normalTexture[pos].xyz);
    S.x_s = S.x_v + candidateRay * candidateRayT;
    S.n_s = OctDecode(candidateHitNormalTexture[pos].xy * 2.0 - 1.0);
    S.Lo = candidateIrradianceTexture[pos].xyz;
    
    return S;
}

Reservoir LoadTemporalReservoir(uint2 pos)
{
    Texture2D reservoirPositionTexture = ResourceDescriptorHeap[c_historyReservoirPosition];
    Texture2D reservoirNormalTexture = ResourceDescriptorHeap[c_historyReservoirNormal];
    Texture2D reservoirRayDirectionTexture = ResourceDescriptorHeap[c_historyReservoirRayDirection];
    Texture2D reservoirSampleNormalTexture = ResourceDescriptorHeap[c_historyReservoirSampleNormal];
    Texture2D reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_historyReservoirSampleRadiance];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_historyReservoir];
    
    float3 rayDirection = OctDecode(reservoirRayDirectionTexture[pos].xy * 2.0 - 1.0);
    float hitT = reservoirPositionTexture[pos].w;

    Reservoir R;
    R.z.x_v = reservoirPositionTexture[pos].xyz;
    R.z.n_v = OctDecode(reservoirNormalTexture[pos].xy * 2.0 - 1.0);
    R.z.x_s = R.z.x_v + rayDirection * hitT;
    R.z.n_s = OctDecode(reservoirSampleNormalTexture[pos].xy * 2.0 - 1.0);
    R.z.Lo = reservoirSampleRadianceTexture[pos].xyz;
    
    R.w_sum = reservoirTexture[pos].x;
    R.M = reservoirTexture[pos].y;
    R.W = reservoirTexture[pos].z;
    
    return R;
}

void StoreTemporalReservoir(uint2 pos, Reservoir R)
{
    RWTexture2D<float4> reservoirPositionTexture = ResourceDescriptorHeap[c_outputReservoirPosition];
    RWTexture2D<float2> reservoirNormalTexture = ResourceDescriptorHeap[c_outputReservoirNormal];
    RWTexture2D<float2> reservoirRayDirectionTexture = ResourceDescriptorHeap[c_outputReservoirRayDirection];
    RWTexture2D<float2> reservoirSampleNormalTexture = ResourceDescriptorHeap[c_outputReservoirSampleNormal];
    RWTexture2D<float3> reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_outputReservoirSampleRadiance];
    RWTexture2D<float4> reservoirTexture = ResourceDescriptorHeap[c_outputReservoir];
    
    float3 ray = R.z.x_s - R.z.x_v;
    float3 rayDirection = normalize(ray);
    float hitT = length(ray);
    
    reservoirPositionTexture[pos] = float4(R.z.x_v, hitT);
    reservoirNormalTexture[pos] = OctEncode(R.z.n_v) * 0.5 + 0.5;
    reservoirRayDirectionTexture[pos] = OctEncode(rayDirection) * 0.5 + 0.5;
    reservoirSampleNormalTexture[pos] = OctEncode(R.z.n_s) * 0.5 + 0.5;
    reservoirSampleRadianceTexture[pos] = R.z.Lo;
    reservoirTexture[pos] = float4(R.w_sum, R.M, R.W, 0.0);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D depthTexture = ResourceDescriptorHeap[c_depth];
    float depth = depthTexture[pos].x;
    if (depth == 0.0)
    {
        return;
    }
    
    PRNG rng = PRNG::Create(pos, SceneCB.renderSize);
    Sample S = LoadInitialSample(pos, depth); // Algorithm 3, line 2
    

    Reservoir R = LoadTemporalReservoir(pos); // Algorithm 3, line 3
    
    float target_p_q = Luminance(S.Lo);
    float p_q = 1.0 / (2.0 * M_PI);
    float w =  target_p_q / p_q;              // Algorithm 3, line 4

    R.Update(S, w, rng.RandomFloat());        // Algorithm 3, line 5
    
    R.M = min(R.M, 30.0);
    R.W = R.w_sum / (R.M * Luminance(R.z.Lo));// Algorithm 3, line 6

    StoreTemporalReservoir(pos, R);           // Algorithm 3, line 7
}
