#pragma once

//#include "../common.hlsli"
#include "../SphericalHarmonics.hlsl"

struct SH
{
    sh2 shY; //2nd order SH
    float co;
    float cg;
    
    void Evaluate(float3 color, float3 dir)
    {
        float3 ycocg = RGBToYCoCg(color);

        shY = shScale(shEvaluate(dir), ycocg.x);
        co = ycocg.y;
        cg = ycocg.z;
    }
    
    float3 Unproject(float3 N)
    {
        float y = max(0.0, shUnproject(shY, N));
        return YCoCgToRGB(float3(y, co, cg));
    }
    
    SH operator+(SH rhs)
    {
        SH sh;
        sh.shY = shAdd(shY, rhs.shY);
        sh.co = co + rhs.co;
        sh.cg = cg + rhs.cg;
        return sh;
    }

    SH operator*(float scale)
    {
        SH sh;
        sh.shY = shScale(shY, scale);
        sh.co = co * scale;
        sh.cg = cg * scale;
        return sh;
    }
};

SH lerp(SH x, SH y, float s)
{
    return y * s + x * (1.0 - s);
}

uint4 PackSH(SH sh)
{
    uint4 packed;
    packed.x = (f32tof16(sh.shY.x) << 16) | f32tof16(sh.shY.y);
    packed.y = (f32tof16(sh.shY.z) << 16) | f32tof16(sh.shY.w);
    packed.z = asuint(sh.co);
    packed.w = asuint(sh.cg);

    return packed;
}

SH UnpackSH(uint4 packed)
{
    SH sh;
    sh.shY.x = f16tof32(packed.x >> 16);
    sh.shY.y = f16tof32(packed.x & 0xffff);
    sh.shY.z = f16tof32(packed.y >> 16);
    sh.shY.w = f16tof32(packed.y & 0xffff);
    sh.co = asfloat(packed.z);
    sh.cg = asfloat(packed.w);

    return sh;
}