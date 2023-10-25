#include "motion_blur_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint2 c_inputTextureSize;
};

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[c_outputTexture];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 maxVelocity = 0.0;
    float maxVelocityLength = 0.0;
    float minVelocityLength = 10000.0;
    
    [unroll]
    for (uint i = 0; i < MOTION_BLUR_TILE_SIZE; ++i)
    {
#ifdef VERTICAL_PASS
        uint2 pos = uint2(dispatchThreadID.x, dispatchThreadID.y * MOTION_BLUR_TILE_SIZE + i);
#else
        uint2 pos = uint2(dispatchThreadID.x * MOTION_BLUR_TILE_SIZE + i, dispatchThreadID.y);
#endif
        
        float2 velocity = inputTexture[clamp(pos, 0, c_inputTextureSize - 1)].xy;
        float length = velocity.x;

        if(length > maxVelocityLength)
        {
            maxVelocityLength = length;
            maxVelocity = velocity;
        }
        
        minVelocityLength = min(minVelocityLength, length);
    }

    outputTexture[dispatchThreadID] = float3(maxVelocity, minVelocityLength);
}
