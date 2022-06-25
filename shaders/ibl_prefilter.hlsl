#include "importance_sampling.hlsli"
#include "random.hlsli"
#include "brdf.hlsli"

// https://www.shadertoy.com/view/3tlBW7
// https://github.com/Nadrin/PBR

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint c_cubeSize;
    uint c_mip;
    uint c_mipCount;
};

static const float3 views[6] =
{
    float3(1.0f, 0.0f, 0.0f),  // X+
    float3(-1.0f, 0.0f, 0.0f), // X-
    float3(0.0f, 1.0f, 0.0f),  // Y+
    float3(0.0f, -1.0f, 0.0f), // Y-
    float3(0.0f, 0.0f, 1.0f),  // Z+
    float3(0.0f, 0.0f, -1.0f), // Z-
};

static const float3 ups[6] =
{
    float3(0.0f, 1.0f, 0.0f),  // X+
    float3(0.0f, 1.0f, 0.0f),  // X-
    float3(0.0f, 0.0f, -1.0f), // Y+
    float3(0.0f, 0.0f, 1.0f),  // Y-
    float3(0.0f, 1.0f, 0.0f),  // Z+
    float3(0.0f, 1.0f, 0.0f),  // Z-
};

static const float3 rights[6] =
{
    float3(0.0f, 0.0f, -1.0f), // X+
    float3(0.0f, 0.0f, 1.0f),  // X-
    float3(1.0f, 0.0f, 0.0f),  // Y+
    float3(1.0f, 0.0f, 0.0f),  // Y-
    float3(1.0f, 0.0f, 0.0f),  // Z+
    float3(-1.0f, 0.0f, 0.0f), // Z-
};

[numthreads(8, 8, 1)]
void specular_filter(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2DArray<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    if (c_mip == 0)
    {
        Texture2DArray inputTexture = ResourceDescriptorHeap[c_inputTexture];
        outputTexture[dispatchThreadID] = inputTexture[dispatchThreadID];
        return;
    }
    
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    TextureCube inputTexture = ResourceDescriptorHeap[c_inputTexture];
    float roughness = float(c_mip) / float(c_mipCount - 1);

    float2 uv = (float2(dispatchThreadID.xy) + 0.5) / (c_cubeSize >> c_mip);
    float2 xy = uv * 2.0f - float2(1.0f, 1.0f);
    xy.y = -xy.y;
    uint slice = dispatchThreadID.z;
    float3 dir = normalize(ups[slice] * xy.y + rights[slice] * xy.x + views[slice]);

    float3 N = dir;
    float3 V = dir;

    float totalWeight = 0.0;
    float3 prefilteredColor = 0.0;

    const uint sampleCount = 64;
    for (uint i = 0; i < sampleCount; ++i)
    {
        float2 random = Hammersley(i, sampleCount);
        float3 H = SampleGGX(random, roughness, N);
        float3 L = reflect(-V, H);

        float NdotL = saturate(dot(N, L));
        if (NdotL > 0.0f)
        {
            float D = D_GGX(N, H, roughness * roughness);
            float NdotH = saturate(dot(N, H));
            float LdotH = saturate(dot(L, H));
            float pdf = D * NdotH / (4 * LdotH);

            // Solid angle represented by this sample
            float omegaS = 1.0 / (float(sampleCount) * pdf);
            // Solid angle covered by 1 pixel
            float omegaP = 4.0 * M_PI / (6.0 * c_cubeSize * c_cubeSize);
            // Original paper suggests biasing the mip to improve the results
            float mipBias = 1.0;
            float mipLevel = max(0.5 * log2(omegaS / omegaP) + mipBias, 0.0);

            prefilteredColor += inputTexture.SampleLevel(linearSampler, L, mipLevel).xyz * NdotL;
            totalWeight += NdotL;
        }
    }

    outputTexture[dispatchThreadID] = float4(prefilteredColor / totalWeight, 1.0);
}

[numthreads(8, 8, 1)]
void diffuse_filter(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    TextureCube inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2DArray<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float2 uv = (float2(dispatchThreadID.xy) + 0.5) / c_cubeSize;
    float2 xy = uv * 2.0f - float2(1.0f, 1.0f);
    xy.y = -xy.y;
    uint slice = dispatchThreadID.z;
    float3 N = normalize(ups[slice] * xy.y + rights[slice] * xy.x + views[slice]);

    float3 irradiance = 0.0;

    const uint sampleCount = 64 * 1024;
    for (uint i = 0; i < sampleCount; ++i)
    {
        float2 random = Hammersley(i, sampleCount);
        float3 L = SampleCosHemisphere(random, N);

        float NdotL = saturate(dot(N, L));
        float pdf = NdotL / M_PI;

        float3 diffuseBRDF = DiffuseBRDF(inputTexture.SampleLevel(linearSampler, L, 0.0).xyz);
        irradiance += diffuseBRDF * NdotL / pdf;
    }

    irradiance /= sampleCount;

    outputTexture[dispatchThreadID] = float4(irradiance, 1.0);
}