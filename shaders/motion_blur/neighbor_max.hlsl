#include "motion_blur_common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
};

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[c_outputTexture];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{    
    float3 maxVelocity = 0.0;
    float maxVelocityLength = 0.0;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float3 velocity = inputTexture[dispatchThreadID + int2(x, y)].xyz;
            float length = velocity.x;

            if (length > maxVelocityLength)
            {
                maxVelocityLength = length;
                maxVelocity = velocity;
            }
        }
    }
    
    outputTexture[dispatchThreadID] = maxVelocity;
}