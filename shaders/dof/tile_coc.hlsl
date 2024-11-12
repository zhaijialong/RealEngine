#include "dof_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
}

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
#if VERTICAL_PASS
static RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
#else
static RWTexture2D<float> outputTexture = ResourceDescriptorHeap[c_outputTexture];
#endif

[numthreads(8, 8, 1)]
void main(int2 dispatchThreadID : SV_DispatchThreadID)
{
    float maxCoc = 0.0;
    
    for (uint i = 0; i < NEAR_COC_TILE_SIZE; ++i)
    {
#ifdef VERTICAL_PASS
        uint2 pos = uint2(dispatchThreadID.x, dispatchThreadID.y * NEAR_COC_TILE_SIZE + i);
        float coc = inputTexture[pos].x;
#else
        uint2 pos = uint2(dispatchThreadID.x * NEAR_COC_TILE_SIZE + i, dispatchThreadID.y);
        float coc = inputTexture[pos].w;
#endif
        
        maxCoc = max(coc, maxCoc);
    }
    
    outputTexture[dispatchThreadID] = maxCoc;
}