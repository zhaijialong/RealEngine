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
    
    for (int i = -8; i <= 8; ++i)
    {
#if VERTICAL_PASS
        float coc = inputTexture[dispatchThreadID + int2(0, i)].x;
#else
        float coc = inputTexture[dispatchThreadID + int2(i, 0)].w;
#endif
        
        maxCoc = max(coc, maxCoc);
    }
    
    outputTexture[dispatchThreadID] = maxCoc;
}