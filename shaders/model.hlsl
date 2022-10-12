#include "model.hlsli"
#include "random.hlsli"
#include "shading_model.hlsli"

model::VertexOutput vs_main(uint vertex_id : SV_VertexID)
{
    model::VertexOutput v = model::GetVertexOutput(c_InstanceIndex, vertex_id);
    return v;
}

struct GBufferOutput
{
    float4 diffuseRT : SV_TARGET0;
    float4 specularRT : SV_TARGET1;
    float4 normalRT : SV_TARGET2;
    float3 emissiveRT : SV_TARGET3;
    float4 customDataRT : SV_TARGET4;
};

GBufferOutput ps_main(model::VertexOutput input
#if !GFX_VENDOR_AMD
    , bool isFrontFace : SV_IsFrontFace //using SV_IsFrontFace with mesh shaders will result in crashes on AMD
#endif
)
{
#if GFX_VENDOR_AMD
    float3 V = normalize(CameraCB.cameraPos - input.worldPos);
    bool isFrontFace = dot(input.normal, V) > 0.0; //workaround for AMD driver bug
#endif

#if UNIFORM_RESOURCE
    uint instanceIndex = c_InstanceIndex;
#else
    uint instanceIndex = input.instanceIndex;
#endif
    
    ShadingModel shadingModel = (ShadingModel)model::GetMaterialConstant(instanceIndex).shadingModel;

    model::PbrMetallicRoughness pbrMetallicRoughness = model::GetMaterialMetallicRoughness(instanceIndex, input.uv);
    model::PbrSpecularGlossiness pbrSpecularGlossiness = model::GetMaterialSpecularGlossiness(instanceIndex, input.uv);
    float3 N = input.normal;
#if NORMAL_TEXTURE
    N = model::GetMaterialNormal(instanceIndex, input.uv, input.tangent, input.bitangent, N);
#endif
#if DOUBLE_SIDED
    N *= isFrontFace ? 1.0 : -1.0;
#endif
    float3 T = model::GetMaterialAnisotropyTangent(instanceIndex, input.uv, input.tangent, input.bitangent, N);
    float ao = model::GetMaterialAO(instanceIndex, input.uv);
    float3 emissive = model::GetMaterialEmissive(instanceIndex, input.uv);
    
    float3 diffuse = float3(1, 1, 1);
    float3 specular = float3(0, 0, 0);
    float roughness = 1.0;
    float alpha = 1.0;
    
#if PBR_METALLIC_ROUGHNESS
    diffuse = pbrMetallicRoughness.albedo * (1.0 - pbrMetallicRoughness.metallic);
    specular = lerp(0.04, pbrMetallicRoughness.albedo, pbrMetallicRoughness.metallic);
    roughness = pbrMetallicRoughness.roughness;
    alpha = pbrMetallicRoughness.alpha;
    ao *= pbrMetallicRoughness.ao;
#endif //PBR_METALLIC_ROUGHNESS

#if PBR_SPECULAR_GLOSSINESS
    specular = pbrSpecularGlossiness.specular;
    diffuse = pbrSpecularGlossiness.diffuse * (1.0 - max(max(specular.r, specular.g), specular.b));
    roughness = 1.0 - pbrSpecularGlossiness.glossiness;
    alpha = pbrSpecularGlossiness.alpha;
#endif //PBR_SPECULAR_GLOSSINESS
    
#if ALPHA_TEST
    clip(alpha - model::GetMaterialConstant(instanceIndex).alphaCutoff);
#endif 

    if (SceneCB.bShowMeshlets)
    {
        uint hash = WangHash(input.meshletIndex);
        diffuse.xyz = float3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0;
        roughness = 1.0;
        emissive = float3(0.0, 0.0, 0.0);
    }
    
    GBufferOutput output;
    output.diffuseRT = float4(diffuse, ao);
    output.specularRT = float4(specular, EncodeShadingModel(shadingModel));
    output.normalRT = float4(EncodeNormal(N), roughness);
    output.emissiveRT = emissive;
    
#if SHADING_MODEL_ANISOTROPY    
    float anisotropy = model::GetMaterialConstant(instanceIndex).anisotropy;
    output.customDataRT = EncodeAnisotropy(T, anisotropy);
#elif SHADING_MODEL_SHEEN
    output.customDataRT = model::GetMaterialSheenColorAndRoughness(instanceIndex, input.uv);
#elif SHADING_MODEL_CLEAR_COAT
    float2 clearCoatAndRoughness = model::GetMaterialClearCoatAndRoughness(instanceIndex, input.uv);
    #if CLEAR_COAT_NORMAL_TEXTURE
        float3 clearCoatNormal =  model::GetMaterialClearCoatNormal(instanceIndex, input.uv, input.tangent, input.bitangent, input.normal);
    #else
        float3 clearCoatNormal = input.normal;
    #endif
    output.normalRT = float4(EncodeNormal(clearCoatNormal), clearCoatAndRoughness.g);
    output.customDataRT = EncodeClearCoat(clearCoatAndRoughness.x, roughness, N);
#elif SHADING_MODEL_HAIR
    output.customDataRT = float4(EncodeNormal(T), 0);
#else
    output.customDataRT = float4(0, 0, 0, 0);
#endif

    return output;
}
