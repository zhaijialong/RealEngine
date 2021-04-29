#include "common.hlsli"

uint bufferIndex;

float4 main( float4 pos : POSITION ) : SV_POSITION
{
    Buffer<float4> buffer = ResourceDescriptorHeap[bufferIndex];
    return foo(buffer[0]);
}