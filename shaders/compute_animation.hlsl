
cbuffer CB : register(b1)
{
    uint c_posBuffer;
    uint c_outputBuffer;
    uint2 _padding;
    
    uint c_boneIDBuffer;
    uint c_boneWeightBuffer;
    uint c_boneMatrixBuffer;
    uint c_boneMatrixBufferOffset;
}

void SkeletalAnimation(inout float4 pos, uint vid, uint boneMatrixBufferOffset) //todo : normal,tangent
{
    StructuredBuffer<uint16_t4> boneIDBuffer = ResourceDescriptorHeap[c_boneIDBuffer];
    StructuredBuffer<float4> boneWeightBuffer = ResourceDescriptorHeap[c_boneWeightBuffer];
    
    uint16_t4 boneID = boneIDBuffer[vid];
    float4 boneWeight = boneWeightBuffer[vid];
    
    ByteAddressBuffer boneMatrixBuffer = ResourceDescriptorHeap[c_boneMatrixBuffer];
    float4x4 boneMatrix0 = boneMatrixBuffer.Load<float4x4>(boneMatrixBufferOffset + sizeof(float4x4) * boneID.x);
    float4x4 boneMatrix1 = boneMatrixBuffer.Load<float4x4>(boneMatrixBufferOffset + sizeof(float4x4) * boneID.y);
    float4x4 boneMatrix2 = boneMatrixBuffer.Load<float4x4>(boneMatrixBufferOffset + sizeof(float4x4) * boneID.z);
    float4x4 boneMatrix3 = boneMatrixBuffer.Load<float4x4>(boneMatrixBufferOffset + sizeof(float4x4) * boneID.w);
    
    pos = mul(boneMatrix0, pos) * boneWeight.x +
        mul(boneMatrix1, pos) * boneWeight.y +
        mul(boneMatrix2, pos) * boneWeight.z +
        mul(boneMatrix3, pos) * boneWeight.w;
    
    pos.w = 1.0;
}

[numthreads(64, 1, 1)]
void skeletal_anim_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[c_posBuffer];
    RWStructuredBuffer<float3> outputBuffer = ResourceDescriptorHeap[c_outputBuffer];
    
    uint vertex_id = dispatchThreadID.x;
    float4 pos = float4(posBuffer[vertex_id], 1.0);

    SkeletalAnimation(pos, vertex_id, c_boneMatrixBufferOffset);
    
    outputBuffer[vertex_id] = pos.xyz;
}