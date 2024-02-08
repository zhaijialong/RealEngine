#include "ray_trace.hlsli"
#include "random.hlsli"
#include "importance_sampling.hlsli"
#include "debug.hlsli"

cbuffer PathTracingConstants : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_maxRayLength;
    uint c_currentSampleIndex;
    uint c_sampleNum;
    
    uint c_outputTexture;
};

float ProbabilityToSampleDiffuse(float3 diffuse, float3 specular)
{
    float lumDiffuse = Luminance(diffuse);
    float lumSpecular = Luminance(specular);
    return lumDiffuse / max(lumDiffuse + lumSpecular, 0.0001);
}

float3 SkyColor(uint2 screenPos)
{
    float3 position = GetWorldPosition(screenPos, 1e-10);
    float3 dir = normalize(position - GetCameraCB().cameraPos);
    
    TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    float3 sky_color = skyTexture.SampleLevel(linearSampler, dir, 0).xyz;

    return sky_color;
}

float2 RandomSample(uint2 pixel, inout uint sampleSet)
{
    uint permuation = sampleSet * SceneCB.renderSize.x * SceneCB.renderSize.y + (pixel.y * SceneCB.renderSize.x + pixel.x);
    ++sampleSet;
    return CMJ(c_currentSampleIndex, c_sampleNum, permuation);
}

[numthreads(8, 8, 1)]
void path_tracing(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float3> emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float depth = depthRT[dispatchThreadID.xy];
    if (depth == 0.0)
    {
        outputTexture[dispatchThreadID.xy] = float4(SkyColor(dispatchThreadID.xy), 1.0);
        return;
    }

    float3 position = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 wo = normalize(GetCameraCB().cameraPos - position);

    float3 diffuse = diffuseRT[dispatchThreadID.xy].xyz;
    float3 specular = specularRT[dispatchThreadID.xy].xyz;
    float3 N = DecodeNormal(normalRT[dispatchThreadID.xy].xyz);
    float roughness = normalRT[dispatchThreadID.xy].w;
    float3 emissive = emissiveRT[dispatchThreadID.xy];

    float3 radiance = 0.0;
    float3 throughput = 1.0;
    float pdf = 1.0;
    float roughness_bias = 0.5 * roughness; //reduce fireflies
    uint sampleSetIndex = 0;

    rt::RayCone cone = rt::RayCone::FromGBuffer(GetLinearDepth(depth));
    
    for (uint i = 0; i < c_maxRayLength + 1; ++i)
    {
        //direct light
        float3 wi = SceneCB.lightDir;

        RayDesc ray;
        ray.Origin = position + N * 0.01;
        ray.Direction = SampleConeUniform(RandomSample(dispatchThreadID.xy, sampleSetIndex), SceneCB.lightRadius, wi);
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;

        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
        float NdotL = saturate(dot(N, wi));
        float3 direct_light = DefaultBRDF(wi, wo, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor * NdotL;
        radiance += (direct_light + emissive) * throughput / pdf;

        if (i == c_maxRayLength)
        {
            break;
        }

        //indirect light
        float probDiffuse = ProbabilityToSampleDiffuse(diffuse, specular);
        float2 randomSample = RandomSample(dispatchThreadID.xy, sampleSetIndex);
        
        if (randomSample.x < probDiffuse)
        {
            randomSample.x /= probDiffuse;
            
            wi = SampleCosHemisphere(randomSample, N); //pdf : NdotL / M_PI

            float3 diffuse_brdf = DiffuseBRDF(diffuse);
            float NdotL = saturate(dot(N, wi));

            throughput *= diffuse_brdf * NdotL;
            pdf *= (NdotL / M_PI) * probDiffuse;
        }
        else
        {
            randomSample.x = (randomSample.x - probDiffuse) / (1.0 - probDiffuse);
            
#define GGX_VNDF 1
#if GGX_VNDF
            float3 H = SampleGGXVNDF(randomSample, roughness, N, wo);
#else
            float3 H = SampleGGX(randomSample, roughness, N);
#endif
            wi = reflect(-wo, H);
            
            roughness = max(roughness, 0.065); //fix reflection artifacts on smooth surface

            float3 F;
            float3 specular_brdf = SpecularBRDF(N, wo, wi, specular, roughness, F);
            float NdotL = saturate(dot(N, wi));

            throughput *= specular_brdf * NdotL;

            float a = roughness * roughness;
            float a2 = a * a;
            float D = D_GGX(N, H, a);
            float NdotH = saturate(dot(N, H));
            float LdotH = saturate(dot(wi, H));
            float NdotV = saturate(dot(N, wo));

#if GGX_VNDF
            float samplePDF = D / (2.0 * (NdotV + sqrt(NdotV * (NdotV - NdotV * a2) + a2)));
#else
            float samplePDF = D * NdotH / (4 * LdotH);
#endif
            pdf *= samplePDF  * (1.0 - probDiffuse);
        }

        ray.Origin = position + N * 0.001;
        ray.Direction = wi;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        rt::HitInfo hitInfo;
        if (rt::TraceRay(ray, hitInfo))
        {
            cone.Propagate(0.0, hitInfo.rayT);
            rt::MaterialData material = rt::GetMaterial(ray, hitInfo, cone);

            position = hitInfo.position;
            wo = -wi;

            diffuse = material.diffuse;
            specular = material.specular;
            N = material.worldNormal;
            roughness = lerp(material.roughness, 1, roughness_bias);
            emissive = material.emissive;
            
            roughness_bias = lerp(roughness_bias, 1, 0.5 * material.roughness);
        }
        else
        {
            TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
            SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
            float3 sky_color = skyTexture.SampleLevel(linearSampler, wi, 0).xyz;

            radiance += sky_color * throughput / pdf;
            break;
        }
    }

    if(any(isnan(radiance)) || any(isinf(radiance)))
    {
        radiance = float3(0, 0, 0);
    }

    outputTexture[dispatchThreadID.xy] = float4(clamp(radiance, 0.0, 65504.0), 1.0);
}

cbuffer AccumulationConstants : register(b1)
{
    uint c_currentColorTexture;
    uint c_currentAlbedoTexture;
    uint c_currentNormalTexture;
    uint c_historyColorTexture;
    uint c_historyAlbedoTexture;
    uint c_historyNormalTexture;
    uint c_outputColorTexture;
    uint c_outputAlebdoTexture;
    uint c_outputNormalTexture;
    uint c_accumulatedFrames;
    uint c_bAccumulationFinished;
};

[numthreads(8, 8, 1)]
void accumulation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D currentColorTexture = ResourceDescriptorHeap[c_currentColorTexture];
    Texture2D currentAlbedoTexture = ResourceDescriptorHeap[c_currentAlbedoTexture];
    Texture2D currentNormalTexture = ResourceDescriptorHeap[c_currentNormalTexture];
    RWTexture2D<float4> historyColorTexture = ResourceDescriptorHeap[c_historyColorTexture];
    RWTexture2D<float4> historyAlbedoTexture = ResourceDescriptorHeap[c_historyAlbedoTexture];
    RWTexture2D<float4> historyNormalTexture = ResourceDescriptorHeap[c_historyNormalTexture];
    RWTexture2D<float4> outputColorTexture = ResourceDescriptorHeap[c_outputColorTexture];
    RWTexture2D<float4> outputAlebdoTexture = ResourceDescriptorHeap[c_outputAlebdoTexture];
    RWTexture2D<float4> outputNormalTexture = ResourceDescriptorHeap[c_outputNormalTexture];
    
    if (c_bAccumulationFinished)
    {
        outputColorTexture[dispatchThreadID.xy] = clamp(historyColorTexture[dispatchThreadID.xy], 0.0, 65504.0);
        outputAlebdoTexture[dispatchThreadID.xy] = historyAlbedoTexture[dispatchThreadID.xy];
        outputNormalTexture[dispatchThreadID.xy] = historyNormalTexture[dispatchThreadID.xy];
    }
    else
    {
        float3 currentColor = currentColorTexture[dispatchThreadID.xy].xyz;
        float3 currentAlbedo = currentAlbedoTexture[dispatchThreadID.xy].xyz;
        float3 currentNormal = DecodeNormal(currentNormalTexture[dispatchThreadID.xy].xyz);
        float3 historyColor = historyColorTexture[dispatchThreadID.xy].xyz;
        float3 historyAlbedo = historyAlbedoTexture[dispatchThreadID.xy].xyz;
        float3 historyNormal = historyNormalTexture[dispatchThreadID.xy].xyz;
        
        float3 outputColor = (c_accumulatedFrames * historyColor + currentColor) / (c_accumulatedFrames + 1);
        float3 outputAlbedo = (c_accumulatedFrames * historyAlbedo + currentAlbedo) / (c_accumulatedFrames + 1);
        float3 outputNormal = (c_accumulatedFrames * historyNormal + currentNormal) / (c_accumulatedFrames + 1);
            
        historyColorTexture[dispatchThreadID.xy] = float4(outputColor, 1.0);
        historyAlbedoTexture[dispatchThreadID.xy] = float4(outputAlbedo, 1.0);
        historyNormalTexture[dispatchThreadID.xy] = float4(outputNormal, 1.0);
        outputColorTexture[dispatchThreadID.xy] = float4(clamp(outputColor, 0.0, 65504.0), 1.0);
        outputAlebdoTexture[dispatchThreadID.xy] = float4(outputAlbedo, 1.0);
        outputNormalTexture[dispatchThreadID.xy] = float4(outputNormal, 1.0);
    }
}
