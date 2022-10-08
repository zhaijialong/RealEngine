#include "brdf.hlsli"
#include "shading_model.hlsli"

cbuffer CB : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_customDataRT;
    uint c_depthRT;
    uint c_shadowRT;
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
    Texture2D customDataRT = ResourceDescriptorHeap[c_customDataRT];
    
    float3 diffuse = diffuseRT[pos].xyz;
    float3 specular = specularRT[pos].xyz;
    ShadingModel shadingModel = DecodeShadingModel(specularRT[pos].w);
    float3 N = DecodeNormal(normalRT[pos].xyz);
    float roughness = normalRT[pos].w;
    float4 customData = customDataRT[pos];

    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);

    //TODO : clustered light culling & shading
    
    Texture2D<float> shadowRT = ResourceDescriptorHeap[c_shadowRT];
    float visibility = shadowRT[pos.xy];
    
    float3 direct_light = 0.0;

    if(visibility > 0)
    {
        float NdotL = saturate(dot(N, SceneCB.lightDir));
        
        switch (shadingModel)
        {
            case ShadingModel::Anisotropy:
            {
                float3 T;
                float anisotropy;
                DecodeAnisotropy(customData, T, anisotropy);
                float3 brdf = AnisotropyBRDF(SceneCB.lightDir, V, N, T, diffuse, specular, roughness, anisotropy);
                direct_light = brdf * visibility * SceneCB.lightColor * NdotL;
                break;
            }
            case ShadingModel::Sheen:
            {
                float3 sheenColor = customData.xyz;
                float sheenRoughness = customData.w;
                float3 sheenBRDF = SheenBRDF(SceneCB.lightDir, V, N, sheenColor, sheenRoughness);
                float sheenScaling = SheenScaling(V, N, sheenColor, sheenRoughness);

                float3 brdf = sheenBRDF + sheenScaling * DefaultBRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness);
                direct_light = brdf * visibility * SceneCB.lightColor * NdotL;
                break;
            }
            case ShadingModel::ClearCoat:
            {
                float clearCoat;
                float baseRoughness;
                float3 baseNormal;
                DecodeClearCoat(customData, clearCoat, baseRoughness, baseNormal);
            
                float clearCoatRoughness = roughness;
                float3 clearCoatNormal = N;
                float3 clearCoatSpecular = float3(0.04, 0.04, 0.04);
            
                float3 clearCoatF;
                float3 clearCoatBRDF = SpecularBRDF(clearCoatNormal, V, SceneCB.lightDir, clearCoatSpecular, clearCoatRoughness, clearCoatF);
                float3 baseBRDF = DefaultBRDF(SceneCB.lightDir, V, baseNormal, diffuse, specular, baseRoughness);
            
                float baseNdotL = saturate(dot(baseNormal, SceneCB.lightDir));
                float3 brdf = baseBRDF * (1.0 - clearCoat * clearCoatF) * baseNdotL + clearCoatBRDF * clearCoat * NdotL;
                direct_light = brdf * visibility * SceneCB.lightColor;
                break;
            }
            case ShadingModel::Hair:
            {
                float3 T = DecodeNormal(customData.xyz);
                float3 hairBsdf = HairBSDF(SceneCB.lightDir, V, T, diffuse, specular, roughness);
                direct_light = hairBsdf * visibility * SceneCB.lightColor;
                break;
            }
            case ShadingModel::Default:
            default:
            {
                float3 brdf = DefaultBRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness);
                direct_light = brdf * visibility * SceneCB.lightColor * NdotL;
                break;
            }
        }
    }

    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];
    outTexture[pos] = float4(direct_light, 1.0);
}