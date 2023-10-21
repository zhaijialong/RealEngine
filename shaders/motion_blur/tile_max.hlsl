#include "../common.hlsli"

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    float2 c_rcpTextureSize;
    uint c_tileSize;
};

static Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
static RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[c_outputTexture];
static SamplerState pointSampler = SamplerDescriptorHeap[SceneCB.pointClampSampler];

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    float2 maxVelocity = 0.0;
    float maxVelocityLengthSq = 0.0;

    for (uint i = 0; i < c_tileSize; ++i)
    {
#ifdef VERTICAL_PASS
        float2 uv = (float2(dispatchThreadID.x, dispatchThreadID.y * c_tileSize + i) + 0.5) * float2(c_rcpTextureSize.x, SceneCB.rcpDisplaySize.y);
#else
        float2 uv = (float2(dispatchThreadID.x * c_tileSize + i, dispatchThreadID.y) + 0.5) * SceneCB.rcpDisplaySize;
#endif
        float2 velocity = inputTexture.SampleLevel(pointSampler, uv, 0.0).xy;
        float length = dot(velocity, velocity);

        if(length > maxVelocityLengthSq)
        {
            maxVelocityLengthSq = length;
            maxVelocity = velocity;
        }
    }

    outputTexture[dispatchThreadID] = maxVelocity;
}
