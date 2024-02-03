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

//https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint PCG(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
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

float RadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse(i));
}

float InterleavedGradientNoise(int2 pos, int frame)
{
    frame = frame % 64;
    float x = float(pos.x) + 5.588238f * float(frame);
    float y = float(pos.y) + 5.588238f * float(frame);
    return frac(52.9829189f * frac(0.06711056f * x + 0.00583715f * y));
}

struct PRNG
{
    uint seed;

    static PRNG Create(uint2 screenPos, uint2 screenSize)
    {
        PRNG rng;
        rng.seed = TEA(uint2(screenPos.x + screenPos.y * screenSize.x, SceneCB.frameIndex), 3);

        return rng;
    }

    uint RandomInt()
    {
        seed = PCG(seed);
        return seed;
    }

    float RandomFloat()
    {
        return asfloat(0x3F800000 | RandomInt() >> 9) - 1.0; //[0, 1)
        //return (float)RandomInt() / float(0xFFFFFFFF); //[0, 1]
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

float GetScalarSTBN(uint2 screenPos, uint frameIndex)
{
    uint3 pos = uint3(screenPos, frameIndex) & uint3(127, 127, 63);

    Texture2DArray scalarSTBN = ResourceDescriptorHeap[SceneCB.scalarSTBN];
    return scalarSTBN.Load(uint4(pos, 0)).x;
}

float3 GetVec3STBN(uint2 screenPos, uint frameIndex)
{
    uint3 pos = uint3(screenPos, frameIndex) & uint3(127, 127, 63);

    Texture2DArray vec3STBN = ResourceDescriptorHeap[SceneCB.vec3STBN];
    return vec3STBN.Load(uint4(pos, 0)).xyz;
}

