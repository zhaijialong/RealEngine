#pragma once

static const float M_PI = 3.141592653f;

float3 DiffuseBRDF(float3 diffuse)
{
    return diffuse; // / M_PI;
}

float3 D_GGX(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
	
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = /*M_PI */denom * denom;
	
    return a2 * rcp(denom);
}

//http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float3 V_SmithGGX(float3 N, float3 V, float3 L, float a)
{
    float a2 = a * a;
    float NoV = saturate(dot(N, V));
    float NoL = saturate(dot(N, L));

    float G_V = NoV + sqrt((NoV - NoV * a2) * NoV + a2);
    float G_L = NoL + sqrt((NoL - NoL * a2) * NoL + a2);
    return rcp(G_V * G_L);
}

float3 F_Schlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H));
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float3 specular, float roughness, out float3 F)
{
    roughness = max(roughness, 0.03);

    float a = roughness * roughness;
    float3 H = normalize(V + L);

    float3 D = D_GGX(N, H, a);
    float3 Vis = V_SmithGGX(N, V, L, a);
    F = F_Schlick(V, H, specular);

    return D * Vis * F;
}

float3 BRDF(float3 L, float3 light_radiance, float3 N, float3 V, float3 diffuse, float3 specular, float roughness)
{
    float NdotL = saturate(dot(N, L));

    float3 F;
    float3 specular_brdf = SpecularBRDF(N, V, L, specular, roughness, F);
    float3 diffuse_brdf = DiffuseBRDF(diffuse) * (1.0 - F);    

    return (diffuse_brdf + specular_brdf) * light_radiance * NdotL;
}