#include "../common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
};

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[c_outputTexture];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{    
    float2 maxVelocity = 0.0;
    float maxVelocityLengthSq = 0.0;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 velocity = inputTexture[dispatchThreadID + int2(x, y)].xy;
            float length = dot(velocity, velocity);

            if (length > maxVelocityLengthSq)
            {
                maxVelocityLengthSq = length;
                maxVelocity = velocity;
            }
        }
    }
    
    outputTexture[dispatchThreadID] = maxVelocity;
}