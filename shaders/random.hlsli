#pragma once

#include "common.hlsli"

//https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint TEA(uint2 v, uint iteration = 8)
{
    uint v0 = v[0], v1 = v[1], s0 = 0;

    [unroll]
    for (uint n = 0; n < iteration; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

//https://en.wikipedia.org/wiki/Linear_congruential_generator
uint LCG(uint x)
{
    return 1664525 * x + 1013904223;
}

//https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint WangHash(uint x)
{
    x = (x ^ 61) ^ (x >> 16);
    x *= 9;
    x = x ^ (x >> 4);
    x *= 0x27d4eb2d;
    x = x ^ (x >> 15);
    return x;
}

//https://en.wikipedia.org/wiki/MurmurHash
uint MurmurHash(uint x)
{
    x ^= x >> 16;
    x *= 0x85ebca6bU;
    x ^= x >> 13;
    x *= 0xc2b2ae35U;
    x ^= x >> 16;
    return x;
}

struct PRNG
{
    uint seed;

    static PRNG Create(uint position)
    {
        PRNG rng;
        rng.seed = TEA(uint2(position, SceneCB.frameIndex), 3);

        return rng;
    }

    uint RandomInt()
    {
        seed = MurmurHash(seed);
        return seed;
    }

    float RandomFloat()
    {
        return (float)RandomInt() / float(0xFFFFFFFF);
    }

    float2 RandomFloat2()
    {
        return float2(RandomFloat(), RandomFloat());
    }

    float3 RandomFloat3()
    {
        return float3(RandomFloat(), RandomFloat(), RandomFloat());
    }
};

// "A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise in Screen Space"
template<uint SPP>
struct BNDS
{
    uint2 pixel;

    static BNDS<SPP> Create(uint2 screenPos, uint2 screenSize)
    {
        BNDS<SPP> rng;

        // Offset retarget for new seeds each frame
        uint2 offset = float2(0.754877669, 0.569840296) * SceneCB.frameIndex * screenSize;
        uint2 offsetId = screenPos + offset;
        rng.pixel.x = offsetId.x % screenSize.x;
		rng.pixel.y = offsetId.y % screenSize.y;

        return rng;
    }

    Texture2D GetScramblingRankingTile();

    float Sample(uint sampleIndex, uint sampleDimension)
    {
        // wrap arguments
        uint pixel_i = pixel.x % 128;
        uint pixel_j = pixel.y % 128;
        sampleIndex = sampleIndex % 256;
        sampleDimension = sampleDimension % 4;

        Texture2D sobolSequenceTexture = ResourceDescriptorHeap[SceneCB.sobolSequenceTexture];
        Texture2D scramblingRankingTile = GetScramblingRankingTile();

        // xor index based on optimized ranking
        uint rankedSampleIndex = sampleIndex ^ (uint)clamp(scramblingRankingTile[uint2(pixel_i, pixel_j)].b * 256.0f, 0, 255);

        // fetch value in sequence
        uint value = (uint)clamp(sobolSequenceTexture[uint2(rankedSampleIndex, 0)][sampleDimension] * 256.0f, 0, 255);

        // If the dimension is optimized, xor sequence value based on optimized scrambling
        value = value ^ (uint)clamp(scramblingRankingTile[uint2(pixel_i, pixel_j)][sampleDimension % 2] * 256.0f, 0, 255);

        // convert to float and return
        float v = (0.5f + value) / 256.0f;
        return v;
    }

    float2 RandomFloat2(uint sampleIndex)
    {
        return float2(Sample(sampleIndex, 0), Sample(sampleIndex, 1));
    }

    float3 RandomFloat3(uint sampleIndex)
    {
        return float3(Sample(sampleIndex, 0), Sample(sampleIndex, 1), Sample(sampleIndex, 2));
    }
};

template<>
Texture2D BNDS<1>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture1SPP];
}

template<>
Texture2D BNDS<2>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture2SPP];
}

template<>
Texture2D BNDS<4>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture4SPP];
}

template<>
Texture2D BNDS<8>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture8SPP];
}

template<>
Texture2D BNDS<16>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture16SPP];
}

template<>
Texture2D BNDS<32>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture32SPP];
}

template<>
Texture2D BNDS<64>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture64SPP];
}

template<>
Texture2D BNDS<128>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture128SPP];
}

template<>
Texture2D BNDS<256>::GetScramblingRankingTile()
{
    return (Texture2D)ResourceDescriptorHeap[SceneCB.scramblingRankingTexture256SPP];
}
