#include "hsr_common.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"
#include "../ray_trace.hlsli"

cbuffer CB : register(b1)
{
    uint c_rayCounterBufferSRV;
    uint c_rayListBufferSRV;
    uint c_normalRT;
    uint c_depthRT;
    uint c_outputTexture;
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
    
    float depth = depthRT[coords];
    float3 position = GetWorldPosition(coords, depth);
    float3 V = normalize(GetCameraCB().cameraPos - position);
    float3 N = DecodeNormal(normalRT[coords].xyz);
    float roughness = normalRT[coords].w;
    
    float3 H = SampleGGXVNDF(GetVec3STBN(coords, SceneCB.frameIndex).xy, roughness, N, V);
    float3 direction = reflect(-V, H);
    
    float3 radiance = float3(0, 0, 0);
    float rayLength = 0.0;
    
    RayDesc ray;
    ray.Origin = position + N * 0.001;
    ray.Direction = direction;
    ray.TMin = 0.001;
    ray.TMax = 1000.0;
    
    rt::RayCone cone = rt::RayCone::FromGBuffer(GetLinearDepth(depth));
    rt::HitInfo hitInfo;

    if (c_bEnableHWRay && rt::TraceRay(ray, hitInfo))
    {
        cone.Propagate(0.0, hitInfo.rayT); // using 0 since no curvature measure at second hit

        rt::MaterialData material = rt::GetMaterial(ray, hitInfo, cone);
        radiance = rt::Shade(hitInfo, material, -direction);
        rayLength = hitInfo.rayT;
    }
    else
    {
        TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
        SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
        radiance = skyTexture.SampleLevel(linearSampler, direction, 0).xyz;
        rayLength = ray.TMax;
    }
    
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