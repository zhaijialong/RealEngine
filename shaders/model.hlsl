#include "common.hlsli"
#include "model_constants.hlsli"
#include "global_constants.hlsli"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
#if NORMAL_TEXTURE
    float3 tangent : TANGENT;
#endif
};

void SkeletalAnimation(inout float4 pos, uint vid) //todo : normal,tangent
{
    StructuredBuffer<uint16_t4> boneIDBuffer = ResourceDescriptorHeap[ModelCB.boneIDBuffer];
    StructuredBuffer<float4> boneWeightBuffer = ResourceDescriptorHeap[ModelCB.boneWeightBuffer];
    
    uint16_t4 boneID = boneIDBuffer[vid];
    float4 boneWeight = boneWeightBuffer[vid];
    
    ByteAddressBuffer boneMatrixBuffer = ResourceDescriptorHeap[ModelCB.boneMatrixBuffer];
    float4x4 boneMatrix0 = boneMatrixBuffer.Load<float4x4>(ModelCB.boneMatrixBufferOffset + sizeof(float4x4) * boneID.x);
    float4x4 boneMatrix1 = boneMatrixBuffer.Load<float4x4>(ModelCB.boneMatrixBufferOffset + sizeof(float4x4) * boneID.y);
    float4x4 boneMatrix2 = boneMatrixBuffer.Load<float4x4>(ModelCB.boneMatrixBufferOffset + sizeof(float4x4) * boneID.z);
    float4x4 boneMatrix3 = boneMatrixBuffer.Load<float4x4>(ModelCB.boneMatrixBufferOffset + sizeof(float4x4) * boneID.w);
    
    pos = mul(boneMatrix0, pos) * boneWeight.x +
        mul(boneMatrix1, pos) * boneWeight.y +
        mul(boneMatrix2, pos) * boneWeight.z +
        mul(boneMatrix3, pos) * boneWeight.w;
    
    pos.w = 1.0;
}

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[ModelCB.posBuffer];
    StructuredBuffer<float2> uvBuffer = ResourceDescriptorHeap[ModelCB.uvBuffer];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[ModelCB.normalBuffer];
    StructuredBuffer<float3> tangentBuffer = ResourceDescriptorHeap[ModelCB.tangentBuffer];
    
    float4 pos = float4(posBuffer[vertex_id], 1.0);
    
#if SKELETAL_ANIMATION
    SkeletalAnimation(pos, vertex_id);
#endif

    VSOutput output;
    output.pos = mul(ModelCB.mtxWVP, pos);
    output.uv = uvBuffer[vertex_id];
    output.normal = mul(ModelCB.mtxNormal, float4(normalBuffer[vertex_id], 0.0f)).xyz;
    
#if NORMAL_TEXTURE
    output.tangent = mul(ModelCB.mtxWorld, float4(tangentBuffer[vertex_id], 0.0f)).xyz;
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
    float3 N = normalize(input.normal);

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
    float3 T = normalize(input.tangent);
    float3 B = normalize(cross(T, N));

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
    output.albedoRT = float4(albedo.xyz, ao);
    output.normalRT = float4(OctNormalEncode(N), roughness, 0.0);
    output.emissiveRT = float4(emissive, metallic);
    
    return output;
}
