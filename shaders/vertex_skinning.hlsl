#include "common.hlsli"
#include "gpu_scene.hlsli"

cbuffer CB : register(b1)
{
    uint c_vertexCount;

    uint c_staticPosBufferAddress;
    uint c_staticNormalBufferAddress;
    uint c_staticTangentBufferAddress;

    uint c_animPosBufferAddress;
    uint c_animNormalBufferAddress;
    uint c_animTangentBufferAddress;

    uint c_jointIDBufferAddress;
    uint c_jointWeightBufferAddress;
    uint c_jointMatrixBufferAddress;
};

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint vertex_id = dispatchThreadID.x;
    if (vertex_id >= c_vertexCount)
    {
        return;
    }

    uint16_t4 jointID = LoadSceneStaticBuffer<uint16_t4>(c_jointIDBufferAddress, vertex_id);
    float4x4 jointMatrix0 = transpose(LoadSceneConstantBuffer<float4x4>(c_jointMatrixBufferAddress + sizeof(float4x4) * jointID.x)); //the transpose is due to DXC's bug
    float4x4 jointMatrix1 = transpose(LoadSceneConstantBuffer<float4x4>(c_jointMatrixBufferAddress + sizeof(float4x4) * jointID.y));
    float4x4 jointMatrix2 = transpose(LoadSceneConstantBuffer<float4x4>(c_jointMatrixBufferAddress + sizeof(float4x4) * jointID.z));
    float4x4 jointMatrix3 = transpose(LoadSceneConstantBuffer<float4x4>(c_jointMatrixBufferAddress + sizeof(float4x4) * jointID.w));
    float4 jointWeight = LoadSceneStaticBuffer<float4>(c_jointWeightBufferAddress, vertex_id);

    float3 pos = LoadSceneStaticBuffer<float3>(c_staticPosBufferAddress, vertex_id);

    float4 skinned_pos = mul(jointMatrix0, float4(pos, 1.0)) * jointWeight.x +
        mul(jointMatrix1, float4(pos, 1.0)) * jointWeight.y +
        mul(jointMatrix2, float4(pos, 1.0)) * jointWeight.z +
        mul(jointMatrix3, float4(pos, 1.0)) * jointWeight.w;

    StoreSceneAnimationBuffer<float3>(c_animPosBufferAddress, vertex_id, skinned_pos.xyz);

    if (c_staticNormalBufferAddress != INVALID_ADDRESS)
    {
        float3 normal = LoadSceneStaticBuffer<float3>(c_staticNormalBufferAddress, vertex_id);

        float4 skinned_normal = mul(jointMatrix0, float4(normal, 0.0)) * jointWeight.x +
            mul(jointMatrix1, float4(normal, 0.0)) * jointWeight.y +
            mul(jointMatrix2, float4(normal, 0.0)) * jointWeight.z +
            mul(jointMatrix3, float4(normal, 0.0)) * jointWeight.w;
        
        StoreSceneAnimationBuffer<float3>(c_animNormalBufferAddress, vertex_id, skinned_normal.xyz);
    }

    if (c_staticTangentBufferAddress != INVALID_ADDRESS)
    {
        float4 tangent = LoadSceneStaticBuffer<float4>(c_staticTangentBufferAddress, vertex_id);

        float4 skinned_tangent = mul(jointMatrix0, float4(tangent.xyz, 0.0)) * jointWeight.x +
            mul(jointMatrix1, float4(tangent.xyz, 0.0)) * jointWeight.y +
            mul(jointMatrix2, float4(tangent.xyz, 0.0)) * jointWeight.z +
            mul(jointMatrix3, float4(tangent.xyz, 0.0)) * jointWeight.w;

        StoreSceneAnimationBuffer<float4>(c_animTangentBufferAddress, vertex_id, float4(skinned_tangent.xyz, tangent.w));
    }
}