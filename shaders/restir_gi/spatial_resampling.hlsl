#include "reservoir.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"

cbuffer CB : register(b1)
{
    uint c_depth;
    uint c_normal;
    uint c_inputReservoirRayDirection;
    uint c_inputReservoirSampleNormal;
    uint c_inputReservoirSampleRadiance;
    uint c_inputReservoir;
    uint c_outputReservoirRayDirection;
    uint c_outputReservoirSampleRadiance;
    uint c_outputReservoir;
}

Reservoir LoadReservoir(uint2 pos, float depth, float3 normal)
{    
    Texture2D reservoirRayDirectionTexture = ResourceDescriptorHeap[c_inputReservoirRayDirection];
    Texture2D reservoirSampleNormalTexture = ResourceDescriptorHeap[c_inputReservoirSampleNormal];
    Texture2D reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_inputReservoirSampleRadiance];
    Texture2D reservoirTexture = ResourceDescriptorHeap[c_inputReservoir];
    
    float3 rayDirection = OctDecode(reservoirRayDirectionTexture[pos].xy * 2.0 - 1.0);
    float hitT = reservoirSampleRadianceTexture[pos].w;

    Reservoir R;
    R.z.x_v = GetWorldPosition(pos, depth);
    R.z.n_v = normal;
    R.z.x_s = R.z.x_v + rayDirection * hitT;
    R.z.n_s = OctDecode(reservoirSampleNormalTexture[pos].xy * 2.0 - 1.0);
    R.z.Lo = reservoirSampleRadianceTexture[pos].xyz;

    R.M = reservoirTexture[pos].x;
    R.W = reservoirTexture[pos].y;
    R.w_sum = R.W * R.M * Luminance(R.z.Lo);
    
    return R;
}

void StoreReservoir(uint2 pos, Reservoir R)
{
    RWTexture2D<float2> reservoirRayDirectionTexture = ResourceDescriptorHeap[c_outputReservoirRayDirection];
    //RWTexture2D<float2> reservoirSampleNormalTexture = ResourceDescriptorHeap[c_outputReservoirSampleNormal];
    RWTexture2D<float4> reservoirSampleRadianceTexture = ResourceDescriptorHeap[c_outputReservoirSampleRadiance];
    RWTexture2D<float2> reservoirTexture = ResourceDescriptorHeap[c_outputReservoir];
    
    float3 ray = R.z.x_s - R.z.x_v;
    float3 rayDirection = normalize(ray);
    float hitT = length(ray);

    reservoirRayDirectionTexture[pos] = OctEncode(rayDirection) * 0.5 + 0.5;
    //reservoirSampleNormalTexture[pos] = OctEncode(R.z.n_s) * 0.5 + 0.5;
    reservoirSampleRadianceTexture[pos] = float4(R.z.Lo, hitT);
    reservoirTexture[pos] = float2(R.M, R.W);
}

// ReSTIR GI paper Eq.11
float GetJacobian(Reservoir q, Reservoir r)
{
    float3 v1 = q.z.x_v - q.z.x_s;
    float3 v2 = r.z.x_v - q.z.x_s;
    float3 n = q.z.n_s;
    
    float a = saturate(dot(normalize(v1), n)) / max(0.00001, saturate(dot(normalize(v2), n)));
    float b = square(length(v1)) / max(0.00001, square(length(v2)));
    return a * b;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    Texture2D depthTexture = ResourceDescriptorHeap[c_depth];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normal];
    
    float depth = depthTexture[pos].x;
    float linearDepth = GetLinearDepth(depth);
    float3 normal = DecodeNormal(normalTexture[pos].xyz);
    
    if (depth == 0.0)
    {
        return;
    }
    
    PRNG rng = PRNG::Create(pos, SceneCB.renderSize);
    Reservoir Rs = LoadReservoir(pos, depth, normal);
    
    const uint maxIterations = 8;
    const float searchRadius = 30.0f; //todo
    float Z = 0.0;
    float selected_target_p = 1.0;
    
    for (uint i = 0; i < maxIterations; ++i)
    {
        uint2 qn = clamp(pos + searchRadius * SampleDiskUniform(rng.RandomFloat2()), 0, SceneCB.renderSize);

        float depth_qn = depthTexture[qn].x;
        float3 normal_qn = DecodeNormal(normalTexture[qn].xyz);

        if (abs(GetLinearDepth(depth_qn) - linearDepth) / linearDepth > 0.05 ||
            saturate(dot(normal, normal_qn)) < 0.9)
        {
            continue;
        }
        
        Reservoir Rn = LoadReservoir(qn, depth_qn, normal_qn);

        float jacobian = GetJacobian(Rn, Rs);
        if (jacobian < 0.01 || isnan(jacobian))
        {
            continue;
        }
        
        float target_p = Luminance(Rn.z.Lo) / jacobian;

        //todo : if Rn's sample point is not visible to xv at q , target_p = 0
        
        if(Rs.Merge(Rn, target_p, rng.RandomFloat()))
        {
            selected_target_p = target_p;
        }

        if (target_p > 0)
        {
            Z += Rn.M;
        }
    }

    if (Z > 0)
    {
        Rs.W = Rs.w_sum / max(0.00001, Z * selected_target_p);
    }
    
    StoreReservoir(pos, Rs);
}