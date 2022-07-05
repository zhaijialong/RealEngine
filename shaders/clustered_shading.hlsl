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
    float3 N = OctNormalDecode(normalRT[pos].xyz);
    float roughness = normalRT[pos].w;
    float4 customData = customDataRT[pos];

    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);

    //TODO : clustered light culling & shading
    
    Texture2D<float> shadowRT = ResourceDescriptorHeap[c_shadowRT];
    float visibility = shadowRT[pos.xy];
    
    float3 BRDF;
    switch (shadingModel)
    {
        case ShadingModel::Anisotropy:
        {
            float3 T;
            float anisotropy;
            DecodeAnisotropy(customData, T, anisotropy);
            BRDF = AnisotropyBRDF(SceneCB.lightDir, V, N, T, diffuse, specular, roughness, anisotropy);
            break;
        }
        case ShadingModel::Sheen:
        {
            float3 sheenColor = customData.xyz;
            float sheenRoughness = customData.w;
            float3 sheenBRDF = SheenBRDF(SceneCB.lightDir, V, N, sheenColor, sheenRoughness);
            float sheenScaling = SheenScaling(SceneCB.lightDir, V, N, sheenColor, sheenRoughness);

            BRDF = sheenBRDF + sheenScaling * DefaultBRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness);
            break;
        }
        case ShadingModel::Default:
        default:
            BRDF = DefaultBRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness);
            break;
    }
    
    float NdotL = saturate(dot(N, SceneCB.lightDir));
    float3 direct_light = BRDF * visibility * SceneCB.lightColor * NdotL;

    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];
    outTexture[pos] = float4(direct_light, 1.0);
}