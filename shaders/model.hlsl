#include "common.hlsli"
#include "model_constants.hlsli"
#include "debug.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
#endif
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{    
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
    StructuredBuffer<float4> tangentBuffer = ResourceDescriptorHeap[ModelCB.tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    float4 worldPos = mul(ModelCB.mtxWorld, pos);

    VSOutput output;
    output.pos = mul(CameraCB.mtxViewProjection, worldPos);
    output.uv = uvBuffer[vertex_id];
    output.normal = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(normalBuffer[vertex_id], 0.0f)).xyz);
    
    if (vertex_id == 0)
    {
        //DrawDebugSphere(ModelCB.center, ModelCB.radius, float3(1, 0, 0));
    }
    
    //DrawDebugLine(worldPos.xyz, worldPos.xyz + output.normal * 0.05, float3(0, 0, 1));
    
#if NORMAL_TEXTURE
    float4 tangent = tangentBuffer[vertex_id];
    output.tangent = normalize(mul(ModelCB.mtxWorldInverseTranspose, float4(tangent.xyz, 0.0f)).xyz);
    output.bitangent = normalize(cross(output.normal, output.tangent) * tangent.w);    
    
    //DrawDebugLine(worldPos.xyz, worldPos.xyz + output.tangent * 0.05, float3(1, 0, 0));
    //DrawDebugLine(worldPos.xyz, worldPos.xyz + output.bitangent * 0.05, float3(0, 1, 0));
#endif
    
    return output;
}

struct GBufferOutput
{
    float4 albedoRT : SV_TARGET0;
    float4 normalRT : SV_TARGET1;
    float4 emissiveRT : SV_TARGET2;
};

GBufferOutput ps_main(VSOutput input)
{
    float3 N = input.normal;

    float4 albedo = float4(MaterialCB.albedo.xyz, 1.0);
    float metallic = MaterialCB.metallic;
    float roughness = MaterialCB.roughness;
    float ao = 1.0;
    float3 emissive = MaterialCB.emissive;

    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.aniso4xSampler];
    
#if ALBEDO_TEXTURE
    Texture2D albedoTexture = ResourceDescriptorHeap[MaterialCB.albedoTexture];
    albedo *= albedoTexture.Sample(linearSampler, input.uv);
#endif
    
#if ALPHA_TEST
    clip(albedo.a - MaterialCB.alphaCutoff);
#endif

#if METALLIC_ROUGHNESS_TEXTURE
    Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[MaterialCB.metallicRoughnessTexture];
    float4 metallicRoughness = metallicRoughnessTexture.Sample(linearSampler, input.uv);
    metallic *= metallicRoughness.b;
    roughness *= metallicRoughness.g;
#if AO_METALLIC_ROUGHNESS_TEXTURE
    ao = metallicRoughness.r;
#endif    
#endif

#if NORMAL_TEXTURE
    float3 T = input.tangent;
    float3 B = input.bitangent;

    Texture2D normalTexture = ResourceDescriptorHeap[MaterialCB.normalTexture];
    float3 normal = normalTexture.Sample(linearSampler, input.uv).xyz;
    
#if RG_NORMAL_TEXTURE
    normal.xy = normal.xy * 2.0 - 1.0;
    normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
#else
    normal = normal * 2.0 - 1.0;
#endif

    N = normalize(normal.x * T + normal.y * B + normal.z * N);
#endif
    
#if EMISSIVE_TEXTURE
    Texture2D emissiveTexture = ResourceDescriptorHeap[MaterialCB.emissiveTexture];
    emissive *= emissiveTexture.Sample(linearSampler, input.uv).xyz;
#endif
    
#if AO_TEXTURE && !AO_METALLIC_ROUGHNESS_TEXTURE
    Texture2D aoTexture = ResourceDescriptorHeap[MaterialCB.aoTexture];
    ao = aoTexture.Sample(linearSampler, input.uv).x;
#endif
    
    GBufferOutput output;
    output.albedoRT = float4(albedo.xyz, metallic);
    output.normalRT = float4(OctNormalEncode(N), roughness);
    output.emissiveRT = float4(emissive, ao);
    
    return output;
}
