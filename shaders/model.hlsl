#include "model.hlsli"
#include "debug.hlsli"

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
};

GBufferOutput ps_main(model::VertexOutput input
#if !GFX_VENDOR_AMD
    , bool isFrontFace : SV_IsFrontFace //using SV_IsFrontFace with mesh shaders will result in crashes on AMD
#endif
)
{
#if GFX_VENDOR_AMD
    bool isFrontFace = true;
#endif

#if UNIFORM_RESOURCE
    input.instanceIndex = c_InstanceIndex;
#endif

    model::PbrMetallicRoughness pbrMetallicRoughness = model::GetMaterialMetallicRoughness(input);
    model::PbrSpecularGlossiness pbrSpecularGlossiness = model::GetMaterialSpecularGlossiness(input);
    float3 N = model::GetMaterialNormal(input, isFrontFace);
    float ao = model::GetMaterialAO(input);
    float3 emissive = model::GetMaterialEmissive(input);
    
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
    clip(alpha - model::GetMaterialConstant(input.instanceIndex).alphaCutoff);
#endif
    
    if (SceneCB.bShowMeshlets)
    {
        uint mhash = Hash(input.meshlet);
        diffuse.xyz = float3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
        roughness = 1.0;
        emissive = float3(0.0, 0.0, 0.0);
    }
    
    GBufferOutput output;
    output.diffuseRT = float4(diffuse, ao);
    output.specularRT = float4(specular, 0);
    output.normalRT = float4(OctNormalEncode(N), roughness);
    output.emissiveRT = emissive;
    
    return output;
}
