#include "shading_model.hlsli"
#include "brdf.hlsli"
#include "gtso.hlsli"
#include "random.hlsli"

cbuffer CB1 : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_directLightingRT;
    uint c_aoRT;
    uint c_indirectSprcularRT;

    uint c_indirectDiffuseRT;
    uint c_customDataRT;
    float c_hsrMaxRoughness;
    uint c_outputRT;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
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
    Texture2D customDataRT = ResourceDescriptorHeap[c_customDataRT];
    
    float4 diffuse = diffuseRT[pos];
    float3 specular = specularRT[pos].xyz;
    float4 normal = normalRT[pos];
    float3 emissive = emissiveRT[pos];
    float4 customData = customDataRT[pos];
    ShadingModel shadingModel = DecodeShadingModel(specularRT[pos].w);
    
    float3 N = DecodeNormal(normal.xyz);
    float roughness = normal.w;
    float ao = diffuse.w;
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(GetCameraCB().cameraPos - worldPos);    
    
    Texture2D directLightingRT = ResourceDescriptorHeap[c_directLightingRT];
    float3 direct_light = directLightingRT[pos].xyz;
    
    Texture2D<uint> aoRT = ResourceDescriptorHeap[c_aoRT];
    
#if GTAO
    #if GTSO
        float gtao;
        float3 bentNormal;
        DecodeVisibilityBentNormal(aoRT[pos], gtao, bentNormal);
    #else
        float gtao = aoRT[pos] / 255.0;
    #endif
    
    ao = min(ao, gtao);
#endif

    float3 indirect_diffuse = DiffuseIBL(N);
    float3 indirect_specular = SpecularIBL(N, V, roughness, specular); //todo : anisotropy

#if DIFFUSE_GI
    Texture2D indirectDiffuseRT = ResourceDescriptorHeap[c_indirectDiffuseRT];
    indirect_diffuse = indirectDiffuseRT[pos].xyz;
    
    if(roughness >= c_hsrMaxRoughness + 0.0001)
    {
        indirect_specular = SpecularIBL(indirect_diffuse, N, V, roughness, specular);
    }
#endif

#if SPECULAR_GI
    if(roughness < c_hsrMaxRoughness + 0.0001)
    {
        Texture2D indirectSprcularRT = ResourceDescriptorHeap[c_indirectSprcularRT];
        indirect_specular = SpecularIBL(indirectSprcularRT[pos].xyz, N, V, roughness, specular);
    }
#endif
    
    if (shadingModel == ShadingModel::Sheen)
    {
        float3 sheenColor = customData.xyz;
        float sheenRoughness = customData.w;
        float sheenScaling = SheenScaling(V, N, sheenColor, sheenRoughness);
        
        indirect_diffuse *= sheenScaling;
        indirect_specular *= sheenScaling;
        
        indirect_specular += SpecularIBL(N, V, sheenRoughness, specular) * sheenColor;
    }
    else if (shadingModel == ShadingModel::ClearCoat)
    {
        float clearCoat;
        float baseRoughness;
        float3 baseNormal;
        DecodeClearCoat(customData, clearCoat, baseRoughness, baseNormal);
        
        indirect_diffuse *= clearCoat;
        indirect_specular *= clearCoat;
        
        float3 clearCoatF = 0.04 + (1 - 0.04) * pow(1 - saturate(dot(N, V)), 5.0);
        indirect_specular += SpecularIBL(baseNormal, V, baseRoughness, specular) * (1.0 - clearCoat * clearCoatF);
        indirect_diffuse += DiffuseIBL(baseNormal) * (1.0 - clearCoat * clearCoatF);
    }

#if GTAO && GTSO
    bentNormal = normalize(mul(GetCameraCB().mtxViewInverse, float4(bentNormal, 0.0)).xyz);
    float3 R = reflect(-V, N);
    float specularAO = ComputeGTSO(R, bentNormal, gtao, roughness);

    indirect_specular *= specularAO;
#endif

    float3 radiance = emissive + direct_light + diffuse.xyz * indirect_diffuse * ao + indirect_specular;
    
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];

#if OUTPUT_DIFFUSE
    outTexture[dispatchThreadID.xy] = float4(diffuse.xyz, 1.0);
#elif OUTPUT_SPECULAR
    outTexture[dispatchThreadID.xy] = float4(specular, 1.0);
#elif OUTPUT_WORLDNORMAL
    outTexture[dispatchThreadID.xy] = float4(N * 0.5 + 0.5, 1.0);
#elif OUTPUT_ROUGHNESS
    outTexture[dispatchThreadID.xy] = float4(roughness, roughness, roughness, 1.0);
#elif OUTPUT_EMISSIVE
    outTexture[dispatchThreadID.xy] = float4(emissive, 1.0);
#elif OUTPUT_SHADING_MODEL
    uint hash = MurmurHash(MurmurHash(MurmurHash((uint)shadingModel)));
    outTexture[dispatchThreadID.xy].xyz = float3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0;
    outTexture[dispatchThreadID.xy].w = 1.0;
#elif OUTPUT_CUSTOM_DATA
    if(shadingModel == ShadingModel::Anisotropy || shadingModel == ShadingModel::Hair)
    {
        float3 T = DecodeNormal(customDataRT[pos].xyz);
        outTexture[dispatchThreadID.xy] = float4(T * 0.5 + 0.5, 1.0);
    }
    else if(shadingModel == ShadingModel::ClearCoat)
    {
        float clearCoat;
        float baseRoughness;
        float3 baseNormal;
        DecodeClearCoat(customDataRT[pos], clearCoat, baseRoughness, baseNormal);
        outTexture[dispatchThreadID.xy] = float4(clearCoat.xxx, 1);
    }
    else
    {
        outTexture[dispatchThreadID.xy] = customDataRT[pos];
    }
#elif OUTPUT_AO
    outTexture[dispatchThreadID.xy] = float4(ao, ao, ao, 1.0);
#elif OUTPUT_DIRECT_LIGHTING
    outTexture[dispatchThreadID.xy] = float4(direct_light, 1.0);
#elif OUTPUT_INDIRECT_SPECULAR
    outTexture[dispatchThreadID.xy] = float4(indirect_specular, 1.0);
#elif OUTPUT_INDIRECT_DIFFUSE
    outTexture[dispatchThreadID.xy] = float4(indirect_diffuse, 1.0);
#else
    outTexture[dispatchThreadID.xy] = float4(radiance, 1.0);
#endif
}