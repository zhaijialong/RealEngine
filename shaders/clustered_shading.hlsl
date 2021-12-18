#include "common.hlsli"

cbuffer CB0 : register(b0)
{
    uint c_width;
    uint c_height;
    float c_invWidth;
    float c_invHeight;
};

cbuffer CB1 : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_shadowRT;
    uint c_gtaoRT;
    uint c_hdrRT;
};

float Shadow(float3 worldPos, float3 worldNormal, float4x4 mtxLightVP, Texture2D shadowRT, SamplerComparisonState shadowSampler)
{
    float4 shadowPos = mul(mtxLightVP, float4(worldPos + worldNormal * 0.001, 1.0));
    shadowPos /= shadowPos.w;
    shadowPos.xy = GetScreenUV(shadowPos.xy);
    
    const float halfTexel = 0.5 / 4096;
    float visibility = shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(halfTexel, halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(-halfTexel, halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(halfTexel, -halfTexel), shadowPos.z).x;
    visibility += shadowRT.SampleCmpLevelZero(shadowSampler, shadowPos.xy + float2(-halfTexel, -halfTexel), shadowPos.z).x;

    return visibility / 4.0f;
}

void DecodeVisibilityBentNormal(const uint packedValue, out float visibility, out float3 bentNormal)
{
    float4 decoded = RGBA8UnormToFloat4(packedValue);
    bentNormal = decoded.xyz * 2.0.xxx - 1.0.xxx; // could normalize - don't want to since it's done so many times, better to do it at the final step only
    visibility = decoded.w;
}

float3 GTAOMultiBounce(float visibility, float3 albedo)
{
    float3 a = 2.0404 * albedo - 0.3324;
    float3 b = -4.7951 * albedo + 0.6417;
    float3 c = 2.7552 * albedo + 0.6903;
    
    float3 x = visibility.xxx;
    return max(x, ((x * a + b) * x + c) * x);
}

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid only in the range 0..1.
 */
float acosFastPositive(float x)
{
    float p = -0.1565827 * x + 1.570796;
    return p * sqrt(1.0 - x);
}

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid in the range -1..1.
 */
float acosFast(float x)
{
    // Lagarde 2014, "Inverse trigonometric functions GPU optimization for AMD GCN architecture"
    // This is the approximation of degree 1, with a max absolute error of 9.0x10^-3
    float y = abs(x);
    float p = -0.1565827 * y + 1.570796;
    p *= sqrt(1.0 - y);
    return x >= 0.0 ? p : M_PI - p;
}

float sphericalCapsIntersection(float cosCap1, float cosCap2, float cosDistance)
{
    // Oat and Sander 2007, "Ambient Aperture Lighting"
    // Approximation mentioned by Jimenez et al. 2016
    float r1 = acosFastPositive(cosCap1);
    float r2 = acosFastPositive(cosCap2);
    float d = acosFast(cosDistance);

    // We work with cosine angles, replace the original paper's use of
    // cos(min(r1, r2)) with max(cosCap1, cosCap2)
    // We also remove a multiplication by 2 * PI to simplify the computation
    // since we divide by 2 * PI in computeBentSpecularAO()

    if (min(r1, r2) <= max(r1, r2) - d)
    {
        return 1.0 - max(cosCap1, cosCap2);
    }
    else if (r1 + r2 <= d)
    {
        return 0.0;
    }

    float delta = abs(r1 - r2);
    float x = 1.0 - saturate((d - delta) / max(r1 + r2 - delta, 1e-4));
    // simplified smoothstep()
    float area = x * x * (-2.0 * x + 3.0);
    return area * (1.0 - max(cosCap1, cosCap2));
}

float GTSO(float3 reflectionDir, float3 bentNormal, float visibility, float roughness)
{
    roughness = max(roughness, 0.03);

    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"

    // aperture from ambient occlusion
    float cosAv = sqrt(1.0 - visibility);
    // aperture from roughness, log(10) / log(2) = 3.321928
    float cosAs = exp2(-3.321928 * roughness * roughness);
    // angle betwen bent normal and reflection direction
    float cosB = dot(bentNormal, reflectionDir);

    // Remove the 2 * PI term from the denominator, it cancels out the same term from
    // sphericalCapsIntersection()
    float ao = sphericalCapsIntersection(cosAv, cosAs, cosB) / (1.0 - cosAs);
    // Smoothly kill specular AO when entering the perceptual roughness range [0.1..0.3]
    // Without this, specular AO can remove all reflections, which looks bad on metals
    return lerp(1.0, ao, smoothstep(0.01, 0.09, roughness));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= c_width || dispatchThreadID.y >= c_height)
    {
        return;
    }
    
    int3 pos = int3(dispatchThreadID.xy, 0);
    
    Texture2D depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT.Load(pos).x;
    if(depth == 0.0)
    {
        return;
    }
    
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    
    float4 diffuse = diffuseRT.Load(pos);
    float3 specular = specularRT.Load(pos).xyz;
    float4 normal = normalRT.Load(pos);
    float3 emissive = emissiveRT.Load(pos).xyz;
    
    float3 N = OctNormalDecode(normal.xyz);
    float roughness = normal.w;
    float ao = diffuse.w;
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);
    
    Texture2D shadowRT = ResourceDescriptorHeap[c_shadowRT];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[SceneCB.shadowSampler];
    
    float visibility = Shadow(worldPos, N, SceneCB.mtxLightVP, shadowRT, shadowSampler);
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse.xyz, specular, roughness) * visibility * SceneCB.lightColor;
    
    Texture2D<uint> gtaoRT = ResourceDescriptorHeap[c_gtaoRT];
    
    float gtao;
    float3 bentNormal;
    DecodeVisibilityBentNormal(gtaoRT.Load(pos), gtao, bentNormal);
    
    ao = min(ao, gtao);
    
    float3 indirect_diffuse = diffuse.xyz * float3(0.15, 0.15, 0.2);

    TextureCube envTexture = ResourceDescriptorHeap[SceneCB.envTexture];
    Texture2D brdfTexture = ResourceDescriptorHeap[SceneCB.brdfTexture];
    
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];
    
    const float MAX_REFLECTION_LOD = 7.0;
    float3 R = reflect(-V, N);
    float3 filtered_env = envTexture.SampleLevel(linearSampler, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    float NdotV = saturate(dot(N, V));
    float2 PreintegratedGF = brdfTexture.Sample(pointSampler, float2(NdotV, roughness)).xy;
    float3 indirect_specular = filtered_env * (specular * PreintegratedGF.x + PreintegratedGF.y);
    
#if GTSO
    bentNormal = normalize(mul(CameraCB.mtxViewInverse, float4(bentNormal, 0.0)).xyz);
    float specularAO = GTSO(R, bentNormal, ao, roughness);
#else
    float specularAO = 1.0;
#endif

    float3 radiance = emissive.xyz + direct_light + indirect_diffuse * ao + indirect_specular * specularAO;
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_hdrRT];    
    outTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
}