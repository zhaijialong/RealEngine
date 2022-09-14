#include "reservoir.hlsli"
#include "../ray_trace.hlsli"
#include "../random.hlsli"
#include "../importance_sampling.hlsli"

cbuffer CB : register(b0)
{
    uint c_halfDepthNormalTexture;
    uint c_prevLinearDepthTexture;
    uint c_historyIrradiance;
    uint c_outputRadianceUAV;
    uint c_outputRayDirection;
}

float3 GetIndirectDiffuseLighting(float3 position, rt::MaterialData material)
{
    if (c_historyIrradiance == INVALID_RESOURCE_INDEX)
    {
        return 0.0;
    }
    
    Texture2D historyIrradianceTexture = ResourceDescriptorHeap[c_historyIrradiance];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    Texture2D prevLinearDepthTexture = ResourceDescriptorHeap[c_prevLinearDepthTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    
    float4 prevClipPos = mul(CameraCB.mtxPrevViewProjection, float4(position, 1.0));
    float3 prevNdcPos = GetNdcPosition(prevClipPos);
    float2 prevUV = GetScreenUV(prevNdcPos.xy);
    float prevLinearDepth = prevLinearDepthTexture.SampleLevel(pointSampler, prevUV, 0.0).x;
    
    if (any(prevUV < 0.0) || any(prevUV > 1.0) ||
        abs(GetLinearDepth(prevNdcPos.z) - prevLinearDepth) > 0.05)
    {
        return 0.0; //todo : maybe some kind of world radiance cache
    }
    
    float3 irradiance = historyIrradianceTexture.SampleLevel(linearSampler, prevUV, 0).xyz;
    return irradiance * material.diffuse;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<uint2> halfDepthNormalTexture = ResourceDescriptorHeap[c_halfDepthNormalTexture];
    RWTexture2D<float4> outputRadianceUAV = ResourceDescriptorHeap[c_outputRadianceUAV];
    RWTexture2D<uint> outputRayDirectionUAV = ResourceDescriptorHeap[c_outputRayDirection];
    
    float depth = asfloat(halfDepthNormalTexture[dispatchThreadID.xy].x);
    float3 N = DecodeNormal16x2(halfDepthNormalTexture[dispatchThreadID.xy].y);

    if (depth == 0.0)
    {
        outputRadianceUAV[dispatchThreadID.xy] = 0.xxxx;
        outputRayDirectionUAV[dispatchThreadID.xy] = 0;
        return;
    }

    float3 worldPos = GetWorldPosition(FullScreenPosition(dispatchThreadID.xy), depth);
    
    BNDS<1> bnds = BNDS<1>::Create(dispatchThreadID.xy, (SceneCB.renderSize + 1) / 2);
    float3 direction = SampleUniformHemisphere(bnds.RandomFloat2(), N); //uniform sample hemisphere, following the ReSTIR GI paper
    float pdf = 1.0 / (2.0 * M_PI);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = direction;
    ray.TMin = 0.00001;
    ray.TMax = 1000.0;

    rt::RayCone cone = rt::RayCone::FromGBuffer(GetLinearDepth(depth));
    rt::HitInfo hitInfo = (rt::HitInfo)0;
    
    float3 radiance = 0.0;
    float3 hitNormal = 0.0;
    
    if (rt::TraceRay(ray, hitInfo))
    {
        cone.Propagate(0.03, hitInfo.rayT);
        rt::MaterialData material = rt::GetMaterial(ray, hitInfo, cone);
        
        RayDesc ray;
        ray.Origin = hitInfo.position + material.worldNormal * 0.01;
        ray.Direction = SceneCB.lightDir;
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;
        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;

        float roughness = lerp(material.roughness, 1.0, 0.5); //reduce fireflies
        
        float3 brdf = DefaultBRDF(SceneCB.lightDir, -direction, material.worldNormal, material.diffuse, material.specular, roughness);
        float3 direct_lighting = brdf * visibility * SceneCB.lightColor * saturate(dot(material.worldNormal, SceneCB.lightDir));
        
        float3 indirect_lighting = GetIndirectDiffuseLighting(hitInfo.position, material);
        
        radiance = material.emissive + direct_lighting + indirect_lighting;
        hitNormal = material.worldNormal;
    }
    else
    {
        TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
        SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
        radiance = skyTexture.SampleLevel(linearSampler, direction, 0).xyz;
        hitNormal = -direction;
    }
    
    outputRadianceUAV[dispatchThreadID.xy] = float4(radiance, 0);
    outputRayDirectionUAV[dispatchThreadID.xy] = EncodeNormal16x2(direction);
}