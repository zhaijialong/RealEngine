#pragma once

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

    static PRNG Create(uint position, uint frameIndex)
    {
        PRNG rng;
        rng.seed = TEA(uint2(position, frameIndex), 3);

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