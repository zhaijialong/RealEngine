#include "hsr_common.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"
#include "../ray_trace.hlsli"
#include "../screen_space_ray_trace.hlsli"
#include "../debug.hlsli"

cbuffer CB : register(b1)
{
    uint c_rayCounterBufferSRV;
    uint c_rayListBufferSRV;
    uint c_normalRT;
    uint c_depthRT;
    uint c_velocityRT;
    uint c_prevSceneColorTexture;
    uint c_outputTexture;

    uint c_hwRayCounterBufferUAV;
    uint c_hwRayListBufferUAV;
}

//based on FFX_SSSR_ValidateHit
bool ValidateHit(ssrt::HitInfo hitInfo, float2 uv, float3 ray_direction)
{ 
    // Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
    float2 manhattan_dist = abs(hitInfo.screenUV - uv);
    float2 screen_size = float2(SceneCB.HZBWidth, SceneCB.HZBHeight);
    if (all(manhattan_dist < (2 / screen_size)))
    {
        return false;
    }
    
    // Don't lookup radiance from the background.
    int2 screenPos = SceneCB.renderSize * hitInfo.screenUV;
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT[screenPos];
    if (depth == 0.0)
    {
        return false;
    }

    // We check if we hit the surface from the back, these should be rejected.
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    float3 hit_normal = OctNormalDecode(normalRT[screenPos].xyz);
    if (dot(hit_normal, ray_direction) > 0)
    {
        return false;
    }
    
    const float depth_thickness = 1.0;
    float distance = abs(GetLinearDepth(depth) - GetLinearDepth(hitInfo.depth));
    float confidence = 1 - smoothstep(0, depth_thickness, distance);
    if (confidence < 0.5)
    {
        return false;
    }

    return true;
}

[numthreads(8, 8, 1)]
void main(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID)
{
    uint ray_index = group_id * 64 + group_index;
    
    Buffer<uint> rayCounterBuffer = ResourceDescriptorHeap[c_rayCounterBufferSRV];
    if (ray_index > rayCounterBuffer[0])
    {
        return;
    }
    
    Buffer<uint> rayListBuffer = ResourceDescriptorHeap[c_rayListBufferSRV];
    uint packed_coords = rayListBuffer[ray_index];
    
    int2 coords;
    bool copy_horizontal;
    bool copy_vertical;
    bool copy_diagonal;
    UnpackRayCoords(packed_coords, coords, copy_horizontal, copy_vertical, copy_diagonal);
    
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];

    float2 uv = GetScreenUV(coords, SceneCB.rcpRenderSize);
    
    float depth = depthRT[coords];
    float3 position = GetWorldPosition(coords, depth);
    float3 V = normalize(CameraCB.cameraPos - position);
    float3 N = OctNormalDecode(normalRT[coords].xyz);
    float roughness = normalRT[coords].w;

    BNDS<1> bnds = BNDS<1>::Create(coords, SceneCB.renderSize);

    float3 H = SampleGGXVNDF(bnds.RandomFloat2(0), roughness, N, V);
    float3 direction = reflect(-V, H);

    float3 radiance = float3(0, 0, 0);
    float rayLength = 0.0;

    ssrt::Ray ray;
    ray.origin = position + N * 0.05;
    ray.direction = direction;
    ray.maxInteration = 32;

    ssrt::HitInfo hitInfo;
    bool valid_hit = c_bEnableSWRay && ssrt::HierarchicalRaymarch(ray, hitInfo) && ValidateHit(hitInfo, uv, direction);
    
    if (valid_hit)
    {
        Texture2D prevSceneColorTexture = ResourceDescriptorHeap[c_prevSceneColorTexture];
        Texture2D velocityRT = ResourceDescriptorHeap[c_velocityRT];
        SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
        
        float2 velocity = velocityRT.SampleLevel(pointSampler, hitInfo.screenUV, 0).xy;
        float2 prevUV = hitInfo.screenUV - velocity * float2(0.5, -0.5); //velocity is in ndc space
        
        radiance = prevSceneColorTexture.SampleLevel(linearSampler, prevUV, 0).xyz;
        
        float3 hitPosition = GetWorldPosition(hitInfo.screenUV, hitInfo.depth);
        rayLength = length(hitPosition - position);
        
        float4 output = float4(radiance, rayLength);
        outputTexture[coords] = output;

        uint2 copy_target = coords ^ 0b1; // Flip last bit to find the mirrored coords along the x and y axis within a quad.
        if (copy_horizontal)
        {
            uint2 copy_coords = uint2(copy_target.x, coords.y);
            outputTexture[copy_coords] = output;
        }
        if (copy_vertical)
        {
            uint2 copy_coords = uint2(coords.x, copy_target.y);
            outputTexture[copy_coords] = output;
        }
        if (copy_diagonal)
        {
            uint2 copy_coords = copy_target;
            outputTexture[copy_coords] = output;
        }
    }

    bool need_hw_ray = !valid_hit;
    uint local_ray_index_in_wave = WavePrefixCountBits(need_hw_ray);
    uint wave_ray_count = WaveActiveCountBits(need_hw_ray);
    uint base_ray_index;
    if (WaveIsFirstLane())
    {
        RWBuffer<uint> hwRayCounterBufferUAV = ResourceDescriptorHeap[c_hwRayCounterBufferUAV];
        InterlockedAdd(hwRayCounterBufferUAV[0], wave_ray_count, base_ray_index);
    }
    
    base_ray_index = WaveReadLaneFirst(base_ray_index);
    
    if (need_hw_ray)
    {
        int ray_index = base_ray_index + local_ray_index_in_wave;
        RWBuffer<uint> hwRayListBufferUAV = ResourceDescriptorHeap[c_hwRayListBufferUAV];
        hwRayListBufferUAV[ray_index] = packed_coords;
    }
}