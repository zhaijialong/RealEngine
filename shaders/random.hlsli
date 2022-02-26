#pragma once

//https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/

struct PRNG
{
    uint seed;

    static PRNG Create(uint position, uint frameIndex)
    {
        PRNG rng;

        uint v0 = position, v1 = frameIndex, s0 = 0;

        [unroll]
        for (uint n = 0; n < 8; n++)
        {
            s0 += 0x9e3779b9;
            v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
            v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
        }
        
        rng.seed = v0;

        return rng;
    }

    uint RandomInt()
    {
        seed = (seed ^ 61) ^ (seed >> 16);
        seed *= 9;
        seed = seed ^ (seed >> 4);
        seed *= 0x27d4eb2d;
        seed = seed ^ (seed >> 15);
        return seed;
    }

    // [0, 1)
    float RandomFloat()
    {
        return float(RandomInt() & 0x00FFFFFF) / float(0x01000000);
    }
};