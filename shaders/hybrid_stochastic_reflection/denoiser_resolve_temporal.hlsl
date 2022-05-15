#include "hsr_common.hlsli"

cbuffer Constants : register(b0)
{
    uint c_tileListBuffer;
    uint c_inputTexture;
    uint c_ouputTexture;
};

#include "ffx-reflection-dnsr/ffx_denoiser_reflections_common.h"

[numthreads(8, 8, 1)]
void main(int2 group_thread_id : SV_GroupThreadID,
                uint group_index : SV_GroupIndex,
                uint group_id : SV_GroupID)
{
    Buffer<uint> tileListBuffer = ResourceDescriptorHeap[c_tileListBuffer];
    uint packed_coords = tileListBuffer[group_id];
    int2 dispatch_thread_id = int2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + group_thread_id;
    int2 dispatch_group_id = dispatch_thread_id / 8;
    uint2 remapped_group_thread_id = FFX_DNSR_Reflections_RemapLane8x8(group_index);
    uint2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;
    
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2D<float4> ouputTexture = ResourceDescriptorHeap[c_ouputTexture];

    ouputTexture[remapped_dispatch_thread_id] = inputTexture[remapped_dispatch_thread_id];
}