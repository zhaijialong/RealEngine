#include "../ray_trace.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"

cbuffer CB : register(b0)
{
    uint c_depthTexture;
    uint c_normalTexture;
    uint c_outputUAV;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[c_depthTexture];
    Texture2D normalTexture = ResourceDescriptorHeap[c_normalTexture];
    RWTexture2D<float4> outputUAV = ResourceDescriptorHeap[c_outputUAV];
    
    float depth = depthTexture[dispatchThreadID.xy];    
    if (depth == 0.0)
    {
        outputUAV[dispatchThreadID.xy] = 0.xxxx;
        return;
    }

    float3 worldPos = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 N = DecodeNormal(normalTexture[dispatchThreadID.xy].xyz);
    
    BNDS<1> bnds = BNDS<1>::Create(dispatchThreadID.xy, SceneCB.renderSize);
    float3 direction = SampleUniformHemisphere(bnds.RandomFloat2(), N); //uniform sample hemisphere, following the ReSTIR GI paper
    
    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = direction;
    ray.TMin = 0.00001;
    ray.TMax = 1000.0;

    rt::RayCone cone = rt::RayCone::FromGBuffer(GetLinearDepth(depth));
    rt::HitInfo hitInfo;
    
    float3 radiance = 0.0;
    
    if (rt::TraceRay(ray, hitInfo))
    {
        cone.Propagate(0.0, hitInfo.rayT); // using 0 since no curvature measure at second hit
        rt::MaterialData material = rt::GetMaterial(ray, hitInfo, cone);
        
        RayDesc ray;
        ray.Origin = hitInfo.position + material.worldNormal * 0.01;
        ray.Direction = SceneCB.lightDir;
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;
        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
        float3 direct_lighting = DefaultBRDF(SceneCB.lightDir, -direction, material.worldNormal, material.diffuse, material.specular, material.roughness) * visibility;
        
        radiance = direct_lighting; //todo : second bounce
    }
    else
    {
        TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
        SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
        radiance = skyTexture.SampleLevel(linearSampler, direction, 0).xyz;
    }
    
    outputUAV[dispatchThreadID.xy] = float4(radiance, 1);
}