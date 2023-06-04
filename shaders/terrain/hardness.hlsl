#include "../common.hlsli"
#include "../random.hlsli"

cbuffer CB : register(b0)
{
    uint c_hardnessSeed;
    uint c_heightmap;
    uint c_hardnessUAV;
    float c_minHardness;
    float c_maxHardness;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    Texture2D<float> heightmap = ResourceDescriptorHeap[c_heightmap];
    RWTexture2D<float> hardnessUAV = ResourceDescriptorHeap[c_hardnessUAV];
    
    uint w, h;
    heightmap.GetDimensions(w, h);
    
    PRNG rng;
    rng.seed = c_hardnessSeed + dispatchThreadID.x + dispatchThreadID.y * w;
    
    float height = heightmap[dispatchThreadID.xy];
    float random = rng.RandomFloat() * 2.0 - 1.0; //[-1, 1)
    float hardness = pow(1.0 - height, 3.0) + random * 0.2;
    
    hardnessUAV[dispatchThreadID.xy] = clamp(hardness, c_minHardness, c_maxHardness);
}

cbuffer BlurCB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint c_horizontalPass;
}

float normpdf(in float x, in float sigma)
{
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

[numthreads(8, 8, 1)]
void blur(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2D<float> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    const int mSize = 25;
    const int kSize = (mSize - 1) / 2;
    const float sigma = 7.0;
    float kernel[mSize];
    
    float Z = 0.0;
    for (int j = 0; j <= kSize; ++j)
    {
        kernel[kSize + j] = kernel[kSize - j] = normpdf(float(j), sigma);
    }

    for (int k = 0; k < mSize; ++k)
    {
        Z += kernel[k];
    }

    float res = 0.0;
    for (int i = -kSize; i <= kSize; ++i)
    {
        int2 pos = c_horizontalPass ? dispatchThreadID.xy + int2(i, 0) : dispatchThreadID.xy + int2(0, i);
        
        res += kernel[kSize + i] * inputTexture[pos];
    }
    
    outputTexture[dispatchThreadID.xy] = res / Z;
}
