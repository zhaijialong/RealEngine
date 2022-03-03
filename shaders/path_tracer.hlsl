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
    uint c_depthTRT;

    uint c_maxRayLength;
    uint c_outputTexture;
};

float ProbabilityToSampleDiffuse(float3 diffuse, float3 specular)
{
    float lumDiffuse = Luminance(diffuse);
    float lumSpecular = Luminance(specular);
    return lumDiffuse / max(lumDiffuse + lumSpecular, 0.0001);
}

[numthreads(8, 8, 1)]
void path_tracing(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float3> emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthTRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float depth = depthRT[dispatchThreadID.xy];
    if (depth == 0.0)
    {
        outputTexture[dispatchThreadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    PRNG rng = PRNG::Create(dispatchThreadID.x + dispatchThreadID.y * SceneCB.viewWidth);

    float3 position = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 wo = normalize(CameraCB.cameraPos - position);

    float3 diffuse = diffuseRT[dispatchThreadID.xy].xyz;
    float3 specular = specularRT[dispatchThreadID.xy].xyz;
    float3 N = OctNormalDecode(normalRT[dispatchThreadID.xy].xyz);
    float roughness = normalRT[dispatchThreadID.xy].w;
    float3 emissive = emissiveRT[dispatchThreadID.xy];

    float3 radiance = 0.0;
    float3 throughput = 1.0;
    float pdf = 1.0;

    for (uint i = 0; i < c_maxRayLength + 1; ++i)
    {
        //direct light
        float3 wi = SceneCB.lightDir;

        RayDesc ray;
        ray.Origin = position + N * 0.01;
        ray.Direction = SampleConeUniform(rng.RandomFloat2(), SceneCB.lightRadius, wi);
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;

        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
        float NdotL = saturate(dot(N, wi));
        float3 direct_light = BRDF(wi, wo, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor * NdotL;
        radiance += (direct_light + emissive) * throughput / pdf;

        if (i == c_maxRayLength)
        {
            break;
        }

        //indirect light
        float probDiffuse = ProbabilityToSampleDiffuse(diffuse, specular);
        if (rng.RandomFloat() < probDiffuse)
        {
            wi = SampleCosHemisphere(rng.RandomFloat2(), N); //pdf : NdotL / M_PI

            float3 diffuse_brdf = DiffuseBRDF(diffuse);
            float NdotL = saturate(dot(N, wi));

            throughput *= diffuse_brdf * NdotL;
            pdf *= (NdotL / M_PI) * probDiffuse;
        }
        else
        {
            float3 H = SampleGGX(rng.RandomFloat2(), roughness, N); //pdf : D * NdotH / (4 * LdotH);
            wi = reflect(-wo, H);
            
            roughness = max(roughness, 0.065); //fix reflection artifacts on smooth surface

            float3 F;
            float3 specular_brdf = SpecularBRDF(N, wo, wi, specular, roughness, F);
            float NdotL = saturate(dot(N, wi));

            throughput *= specular_brdf * NdotL;

            float D = D_GGX(N, H, roughness * roughness);
            float NdotH = saturate(dot(N, H));
            float LdotH = saturate(dot(wi, H));

            pdf *= (D * NdotH / (4 * LdotH)) * (1.0 - probDiffuse);
        }

        //firefly rejection
        if (max(max(throughput.x, throughput.y), throughput.z) / pdf > 100.0)
        {
            break;
        }

        ray.Origin = position + N * 0.001;
        ray.Direction = wi;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        rt::HitInfo hitInfo;
        if (rt::TraceRay(ray, hitInfo))
        {
            rt::MaterialData material = rt::GetMaterial(hitInfo);

            position = hitInfo.position;
            wo = -wi;

            diffuse = material.diffuse;
            specular = material.specular;
            N = material.worldNormal;
            roughness = material.roughness;
            emissive = material.emissive;
        }
        else
        {
            TextureCube envTexture = ResourceDescriptorHeap[SceneCB.envTexture];
            SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
            float3 sky_color = envTexture.SampleLevel(linearSampler, wi, 0).xyz;

            radiance += sky_color * throughput / pdf;
            break;
        }
    }

    if(any(isnan(radiance)) || any(isinf(radiance)))
    {
        radiance = float3(0, 0, 0);
    }

    outputTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}

cbuffer AccumulationConstants : register(b0)
{
    uint c_currentFrameTexture;
    uint c_historyTexture;
    uint c_accumulationTexture;
    uint c_accumulatedFrames;
    uint c_bEnableAccumulation;
};

[numthreads(8, 8, 1)]
void accumulation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D currentFrameTexture = ResourceDescriptorHeap[c_currentFrameTexture];
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[c_accumulationTexture];

    float3 current = currentFrameTexture[dispatchThreadID.xy].xyz;
    float3 output = current;

    if (c_bEnableAccumulation)
    {
        RWTexture2D<float4> historyTexture = ResourceDescriptorHeap[c_historyTexture];
        float3 history = historyTexture[dispatchThreadID.xy].xyz;

        output = (c_accumulatedFrames * history + current) / (c_accumulatedFrames + 1);
        historyTexture[dispatchThreadID.xy] = float4(output, 1.0);
    }

    accumulationTexture[dispatchThreadID.xy] = float4(clamp(output, 0.0, 65504.0), 1.0);
}