#pragma once

//#include "../common.hlsli"
#include "../SphericalHarmonics.hlsl"

#define MAX_TEMPORAL_ACCUMULATION_FRAME (32)

// "Precomputed Global Illumination in Frostbite", Yuriy O'Donnell, 2018
sh2 shApplyDiffuseConvolutionL1(sh2 sh)
{
    float A0 = 0.886227f; // pi / sqrt(fourPi)
    float A1 = 1.023326f; // sqrt(pi / 3)
    
    sh2 result;
    result.x = sh.x * A0;
    result.yzw = sh.yzw * A1;

    return result;
}

struct SH
{
    sh2 shY; //2nd order SH
    float co;
    float cg;
    
    void Project(float3 color, float3 dir)
    {
        float3 ycocg = RGBToYCoCg(color);
        
        shY = 2.0 * M_PI * shEvaluate(dir) * ycocg.x; //radiance SH
        co = ycocg.y;
        cg = ycocg.z;
    }
    
    float3 UnprojectIrradiance(float3 N)
    {
        sh2 irradianceSH = shApplyDiffuseConvolutionL1(shY);
        
        float y = max(0.0, shUnproject(irradianceSH, N));
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
    packed.z = asuint(sh.co); //todo : maybe seperate cocg to another rg16f texture, to save some bandwidth...
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