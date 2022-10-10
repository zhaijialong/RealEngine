#pragma once

#include "common.hlsli"

#define MIN_ROUGHNESS (0.03)

float3 DiffuseBRDF(float3 diffuse)
{
    return diffuse / M_PI;
}

float D_GGX(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;
    
    return a2 * rcp(denom);
}

float D_GGX_Aniso(float ax, float ay, float3 T, float3 B, float3 N, float3 H)
{
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    
    float a2 = ax * ay;
    float3 d = float3(ay * TdotH, ax * BdotH, a2 * NdotH);
    float d2 = dot(d, d);
    float b2 = a2 / d2;
    return a2 * b2 * b2 * (1.0 / M_PI);
}

//http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float V_SmithGGX(float3 N, float3 V, float3 L, float a)
{
    float a2 = a * a;
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    float G_V = NdotV + sqrt((NdotV - NdotV * a2) * NdotV + a2);
    float G_L = NdotL + sqrt((NdotL - NdotL * a2) * NdotL + a2);
    return rcp(G_V * G_L);
}

float V_SmithGGX_Aniso(float ax, float ay, float3 T, float3 B, float3 N, float3 V, float3 L)
{
    float TdotV = dot(T, V);
    float TdotL = dot(T, L);
    float BdotV = dot(B, V);
    float BdotL = dot(B, L);
    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    
    float G_V = NdotL * length(float3(ax * TdotV, ay * BdotV, NdotV));
    float G_L = NdotV * length(float3(ax * TdotL, ay * BdotL, NdotL));
    return 0.5 * rcp(G_V + G_L);
}

float3 F_Schlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H));
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float3 specular, float roughness, out float3 F)
{
    roughness = max(roughness, MIN_ROUGHNESS);

    float a = roughness * roughness;
    float3 H = normalize(V + L);

    float D = D_GGX(N, H, a);
    float Vis = V_SmithGGX(N, V, L, a);
    F = F_Schlick(V, H, specular);

    return D * Vis * F;
}

float3 DefaultBRDF(float3 L, float3 V, float3 N, float3 diffuse, float3 specular, float roughness)
{
    float3 F;
    float3 specular_brdf = SpecularBRDF(N, V, L, specular, roughness, F);
    float3 diffuse_brdf = DiffuseBRDF(diffuse) * (1.0 - F);

    return diffuse_brdf + specular_brdf;
}

float3 AnisotropyBRDF(float3 L, float3 V, float3 N, float3 T, float3 diffuse, float3 specular, float roughness, float anisotropy)
{
    float a = roughness * roughness;
    float ax = max(a * (1.0 + anisotropy), MIN_ROUGHNESS);
    float ay = max(a * (1.0 - anisotropy), MIN_ROUGHNESS);
    
    float3 B = cross(N, T);
    float3 H = normalize(V + L);    
    
    float3 D = D_GGX_Aniso(ax, ay, T, B, N, H);
    float3 Vis = V_SmithGGX_Aniso(ax, ay, T, B, N, V, L);
    float3 F = F_Schlick(V, H, specular);
    
    return DiffuseBRDF(diffuse) * (1.0 - F) + D * Vis * F;
}


// Estevez and Kulla 2017, "Production Friendly Microfacet Sheen BRDF"
float D_Charlie(float NdotH, float alpha)
{
    float invR = 1.0 / alpha;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}

// Michael Ashikhmin, Simon Premoze, "Distribution-based BRDFs", 2007
float V_Ashikhmin(float NdotL, float NdotV)
{
    return 1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV));
}

float3 SheenBRDF(float3 L, float3 V, float3 N, float3 sheenColor, float sheenRoughness)
{
    sheenRoughness = max(sheenRoughness, MIN_ROUGHNESS);
    float alpha = sheenRoughness * sheenRoughness;
    
    float3 H = normalize(V + L);
    float NdotH = saturate(dot(N, H));
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    
    return sheenColor * D_Charlie(NdotH, alpha) * V_Ashikhmin(NdotL, NdotV);
}

float SheenE(float cosTheta, float sheenRoughness)
{
    Texture2D sheenETexture = ResourceDescriptorHeap[SceneCB.sheenETexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    return sheenETexture.SampleLevel(linearSampler, float2(cosTheta, sheenRoughness * sheenRoughness), 0).x;
}

float SheenScaling(float3 V, float3 N, float3 sheenColor, float sheenRoughness)
{
    float NdotV = saturate(dot(N, V));
    
    return 1.0 - max3(sheenColor) * SheenE(NdotV, sheenRoughness);
}

float3 HairBSDF(float3 L, float3 V, float3 T, float3 diffuse, float3 specular, float roughness)
{
    Texture3D marschnerTextureM = ResourceDescriptorHeap[SceneCB.marschnerTextureM];
    Texture2D marschnerTextureN = ResourceDescriptorHeap[SceneCB.marschnerTextureN];
    SamplerState bilinearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    float sinThetaL = clamp(dot(T, L), -1.0, 1.0);
    float sinThetaV = clamp(dot(T, V), -1.0, 1.0);

    float3 uvM = float3(sinThetaL * 0.5 + 0.5, sinThetaV * 0.5 + 0.5, roughness);
    float4 M = marschnerTextureM.SampleLevel(bilinearSampler, uvM, 0.0);
    
    float3 Lperp = L - sinThetaL * T;
    float3 Vperp = V - sinThetaV * T;
    float cosPhi = dot(Vperp, Lperp) * rsqrt(dot(Vperp, Vperp) * dot(Lperp, Lperp));
    float cosThetaD = M.w; //[0, 1]

    float2 uvN = float2(cosPhi * 0.5 + 0.5, cosThetaD);
    float4 N = marschnerTextureN.SampleLevel(bilinearSampler, uvN, 0.0);
    
    float3 S = dot(M.xyz, N.xyz) * diffuse / (cosThetaD * cosThetaD);
    
    //todo : multi scatter ?
    return S;
}