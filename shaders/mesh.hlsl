#include "common.hlsli"

uint bufferIndex;

float4 vs_main( uint vertex_id : SV_VertexID ) : SV_POSITION
{
    Buffer<float4> buffer = ResourceDescriptorHeap[bufferIndex];
    return foo(buffer[vertex_id]);
}

float4 ps_main() : SV_TARGET
{
    return float4(1.0, 0.0, 0.0, 1.0);
}