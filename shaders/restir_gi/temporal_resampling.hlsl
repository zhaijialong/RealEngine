#include "reservoir.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b1)
{
    uint c_depth;
    uint c_normal;
    uint c_velocity;
    uint c_prevLinearDepth;
    uint c_prevNormal;
    uint c_candidateRadiance;
    uint c_candidateHitNormal;
    uint c_candidateRay;
    uint c_historyReservoirRayDirection;
    uint c_historyReservoirSampleNormal;
    uint c_historyReservoirSampleRadiance;
    uint c_historyReservoir;
    uint c_outputReservoirRayDirection;
    uint c_outputReservoirSampleNormal;
    uint c_outputReservoirSampleRadiance;
    uint c_outputReservoir;
}

Sample LoadInitialSample(uint2 pos, float depth)
{
    Texture2D normalTexture = ResourceDescriptorHeap[c_normal];
    Texture2D candidateRadianceTexture = ResourceDescriptorHeap[c_candidateRadiance];
    Texture2D candidateHitNormalTexture = ResourceDescriptorHeap[c_candidateHitNormal];
    Texture2D candidateRayTexture = ResourceDescriptorHeap[c_candidateRay];
    
    float3 candidateRay = OctDecode(candidateRayTexture[pos].xy * 2.0 - 1.0);
    float candidateRayT = candidateRadianceTexture[pos].w;
    
    Sample S;
    S.x_v = GetWorldPosition(pos, depth);
    S.n_v = DecodeNormal(normalTexture[pos].xyz);
    S.x_s = S.x_v + candidateRay * candidateRayT;
    S.n_s = OctDecode(candidateHitNormalTexture[pos].xy * 2.0 - 1.0);
    S.Lo = candidateRadianceTexture[pos].xyz;
    
    return S;
}

Reservoir LoadTemporalReservoir(uint2 pos)
{
    Texture2D prevLinearDepthTexture = ResourceDescriptorHeap[c_prevLinearDepth];
    Texture2D prevNormalTexture = ResourceDescriptorHeap[c_prevNormal];
    
    Texture2D reservoirRayDirectionTexture = ResourceDescriptorHeap[c_historyReservoirRayDirection];
    Texture2D reservoirSampleNormalTexture = ResourceDescriptorHeap[c_historyReservoirSampleNormal];
    Texture2D reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_historyReservoirSampleRadiance];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_historyReservoir];
    
    float depth = GetNdcDepth(prevLinearDepthTexture[pos].x);
    float2 screenUV = GetScreenUV(pos, SceneCB.rcpRenderSize);
    float4 clipPos = float4((screenUV * 2.0 - 1.0) * float2(1.0, -1.0), depth, 1.0);
    float4 worldPos = mul(CameraCB.mtxPrevViewProjectionInverse, clipPos);
    worldPos.xyz /= worldPos.w;
    
    float3 rayDirection = OctDecode(reservoirRayDirectionTexture[pos].xy * 2.0 - 1.0);
    float hitT = reservoirSampleRadianceTexture[pos].w;

    Reservoir R;
    R.z.x_v = worldPos.xyz;
    R.z.n_v = DecodeNormal(prevNormalTexture[pos].xyz);
    R.z.x_s = R.z.x_v + rayDirection * hitT;
    R.z.n_s = OctDecode(reservoirSampleNormalTexture[pos].xy * 2.0 - 1.0);
    R.z.Lo = reservoirSampleRadianceTexture[pos].xyz;

    R.M = reservoirTexture[pos].x;
    R.W = reservoirTexture[pos].y;
    R.w_sum = R.W * R.M * Luminance(R.z.Lo);
    
    return R;
}

void StoreTemporalReservoir(uint2 pos, Reservoir R)
{
    RWTexture2D<float2> reservoirRayDirectionTexture = ResourceDescriptorHeap[c_outputReservoirRayDirection];
    RWTexture2D<float2> reservoirSampleNormalTexture = ResourceDescriptorHeap[c_outputReservoirSampleNormal];
    RWTexture2D<float4> reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_outputReservoirSampleRadiance];
    RWTexture2D<float2> reservoirTexture = ResourceDescriptorHeap[c_outputReservoir];
    
    float3 ray = R.z.x_s - R.z.x_v;
    float3 rayDirection = normalize(ray);
    float hitT = length(ray);

    reservoirRayDirectionTexture[pos] = OctEncode(rayDirection) * 0.5 + 0.5;
    reservoirSampleNormalTexture[pos] = OctEncode(R.z.n_s) * 0.5 + 0.5;
    reservoirSampleRadianceTexture[pos] = float4(R.z.Lo, hitT);
    reservoirTexture[pos] = float2(R.M, R.W);
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
    Sample S = LoadInitialSample(pos, depth);
    
    Reservoir R = (Reservoir)0;
    
    Texture2D velocityTexture = ResourceDescriptorHeap[c_velocity];
    float2 velocity = velocityTexture[pos].xy;
    float2 prevUV = GetScreenUV(pos, SceneCB.rcpRenderSize) - velocity * float2(0.5, -0.5);
    bool outOfBound = any(prevUV < 0.0) || any(prevUV > 1.0);
    
    if (!outOfBound)
    {
        uint2 prevPos = (uint2)floor(prevUV * SceneCB.renderSize);
        R = LoadTemporalReservoir(prevPos);
        
        bool invalid = false;
        invalid |= length(S.x_v - R.z.x_v) > 0.1 * GetLinearDepth(depth);
        invalid |= saturate(dot(S.n_v, R.z.n_v)) < 0.8;
        
        if (invalid)
        {
            R.w_sum = R.M = 0;
        }
    }
    
    float target_p_q = Luminance(S.Lo);
    float p_q = 1.0 / (2.0 * M_PI);
    float w = target_p_q / p_q;

    R.Update(S, w, rng.RandomFloat());
    R.W = R.w_sum / max(0.00001, R.M * Luminance(R.z.Lo));
    R.M = min(R.M, 30.0);
    
    StoreTemporalReservoir(pos, R);
}
