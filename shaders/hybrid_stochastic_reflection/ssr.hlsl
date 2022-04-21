#include "../random.hlsli"
#include "../importance_sampling.hlsli"
#include "../ray_trace.hlsli"
#include "../screen_space_ray_trace.hlsli"
#include "../debug.hlsli"

cbuffer CB : register(b1)
{
    uint c_normalRT;
    uint c_depthRT;
    uint c_sceneHZB;
    uint c_velocityRT;
    uint c_prevSceneColorTexture;
    uint c_outputTexture;
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
    int2 screenPos = int2(SceneCB.viewWidth, SceneCB.viewHeight) * hitInfo.screenUV;
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
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];

    float2 uv = GetScreenUV(dispatchThreadID.xy);
    
    float depth = depthRT[dispatchThreadID.xy];
    if (depth == 0.0)
    {
        outputTexture[dispatchThreadID.xy] = float3(0.0, 0.0, 0.0);
        return;
    }
    
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.maxReductionSampler];
    //depth = depthRT.SampleLevel(maxReductionSampler, uv, 0);

    float3 position = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - position);
    float3 N = OctNormalDecode(normalRT[dispatchThreadID.xy].xyz);
    float roughness = normalRT[dispatchThreadID.xy].w;

    BNDS<1> bnds = BNDS<1>::Create(dispatchThreadID.xy, uint2(SceneCB.viewWidth, SceneCB.viewHeight));

    float3 H = SampleGGXVNDF(bnds.RandomFloat2(0), roughness, N, V);
    float3 direction = reflect(-V, H);

    float3 radiance = float3(0, 0, 0);


    ssrt::Ray ray;
    ray.origin = position + N * 0.05;
    ray.direction = direction;
    ray.maxInteration = 32;

    ssrt::HitInfo hitInfo;
    if (ssrt::HierarchicalRaymarch(ray, hitInfo) && ValidateHit(hitInfo, uv, direction))
    {
        Texture2D prevSceneColorTexture = ResourceDescriptorHeap[c_prevSceneColorTexture];
        Texture2D velocityRT = ResourceDescriptorHeap[c_velocityRT];
        SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
        
        float2 velocity = velocityRT.SampleLevel(pointSampler, hitInfo.screenUV, 0).xy;
        float2 prevUV = hitInfo.screenUV - velocity * float2(0.5, -0.5); //velocity is in ndc space
        
        radiance = prevSceneColorTexture.SampleLevel(linearSampler, prevUV, 0).xyz;
    }
    else
    {
        //todo : split to a seperate pass
        RayDesc ray;
        ray.Origin = position + N * 0.001;
        ray.Direction = direction;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        rt::HitInfo hitInfo;
        if (rt::TraceRay(ray, hitInfo))
        {
            rt::MaterialData material = rt::GetMaterial(hitInfo);
            radiance = rt::Shade(hitInfo, material, -direction);
        }
        else
        {
            TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
            radiance = skyTexture.SampleLevel(linearSampler, direction, 0).xyz;
        }        
    }
    
    outputTexture[dispatchThreadID.xy] = radiance;
}