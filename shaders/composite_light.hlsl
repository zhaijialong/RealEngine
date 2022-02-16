#include "common.hlsli"

cbuffer CB1 : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_shadowRT;
    uint c_aoRT;
    uint c_ouputRT;
};

void DecodeVisibilityBentNormal(const uint packedValue, out float visibility, out float3 bentNormal)
{
    float4 decoded = RGBA8UnormToFloat4(packedValue);
    bentNormal = decoded.xyz * 2.0.xxx - 1.0.xxx;
    visibility = decoded.w;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= SceneCB.viewWidth || dispatchThreadID.y >= SceneCB.viewHeight)
    {
        return;
    }
    
    int2 pos = dispatchThreadID.xy;
    
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT[pos];
    if (depth == 0.0)
    {
        return;
    }
    
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float3> emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    
    float4 diffuse = diffuseRT[pos];
    float3 specular = specularRT[pos].xyz;
    float4 normal = normalRT[pos];
    float3 emissive = emissiveRT[pos];
    
    float3 N = OctNormalDecode(normal.xyz);
    float roughness = normal.w;
    float ao = diffuse.w;
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);
    
    Texture2D<float> shadowRT = ResourceDescriptorHeap[c_shadowRT];
    float visibility = shadowRT[pos.xy];
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse.xyz, specular, roughness) * visibility * SceneCB.lightColor;
    
    Texture2D<uint> aoRT = ResourceDescriptorHeap[c_aoRT];
    
#if GTAO
    float gtao;
    float3 bentNormal;
    DecodeVisibilityBentNormal(aoRT[pos], gtao, bentNormal);
    
    ao = min(ao, gtao);
#endif
    
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
    
#if GTAO && GTSO
    bentNormal = normalize(mul(CameraCB.mtxViewInverse, float4(bentNormal, 0.0)).xyz);
    float specularAO = GTSO(R, bentNormal, ao, roughness);
#else
    float specularAO = 1.0;
#endif

    float3 radiance = emissive + direct_light + indirect_diffuse * ao + indirect_specular * specularAO;
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_ouputRT];

#if OUTPUT_DIFFUSE
    outTexture[dispatchThreadID.xy] = float4(diffuse.xyz, 1.0);
#elif OUTPUT_SPECULAR
    outTexture[dispatchThreadID.xy] = float4(specular, 1.0);
#elif OUTPUT_WORLDNORMAL
    outTexture[dispatchThreadID.xy] = float4(N * 0.5 + 0.5, 1.0);
#elif OUTPUT_EMISSIVE
    outTexture[dispatchThreadID.xy] = float4(emissive, 1.0);
#elif OUTPUT_AO
    outTexture[dispatchThreadID.xy] = float4(ao, ao, ao, 1.0);
#elif OUTPUT_SHADOW
    outTexture[dispatchThreadID.xy] = float4(visibility, visibility, visibility, 1.0);
#else
    outTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
#endif
}