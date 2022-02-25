#include "bloom.hlsli"

struct DownsampleConstants
{
    float2 inputPixelSize; //high mip
    float2 outputPixelSize; //low mip

    uint inputTexture;
    uint outputTexture;
};
ConstantBuffer<DownsampleConstants> DownsampleCB : register(b1);

[numthreads(8, 8, 1)]
void downsampling(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[DownsampleCB.inputTexture];
    RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[DownsampleCB.outputTexture];

    float2 uv = ((float2)dispatchThreadID.xy + 0.5) * DownsampleCB.outputPixelSize;

    outputTexture[dispatchThreadID.xy] = BloomDownsample(inputTexture, uv, DownsampleCB.inputPixelSize);
}

struct UpsampleConstants
{
    float2 inputPixelSize; //low mip
    float2 outputPixelSize; //high mip

    uint inputLowTexture;
    uint inputHighTexture;
    uint outputTexture;
};
ConstantBuffer<UpsampleConstants> UpsampleCB : register(b1);

[numthreads(8, 8, 1)]
void upsampling(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputLowTexture = ResourceDescriptorHeap[UpsampleCB.inputLowTexture];
    Texture2D inputHighTexture = ResourceDescriptorHeap[UpsampleCB.inputHighTexture];
    RWTexture2D<float3> outputTexture = ResourceDescriptorHeap[UpsampleCB.outputTexture];

    float2 uv = ((float2)dispatchThreadID.xy + 0.5) * UpsampleCB.outputPixelSize;

    outputTexture[dispatchThreadID.xy] = inputHighTexture[dispatchThreadID.xy].xyz + BloomUpsample(inputLowTexture, uv, UpsampleCB.inputPixelSize);
}