cbuffer cb : register(b1)
{
    uint c_input;
    uint c_output;
    uint2 __padding;
    
    uint4 const0;
    uint4 const1;
};

struct TextureIO
{
    Texture2D InputTexture;
    RWTexture2D<float4> OutputTexture;
};

static TextureIO io;

#define A_GPU 1
#define A_HLSL 1

#if CAS_FP16
#define A_HALF 1
#define CAS_PACKED_ONLY 1
#endif //CAS_FP16

#include "ffx_a.h"

#if CAS_FP16

AH3 CasLoadH(ASW2 p)
{
    return io.InputTexture.Load(ASU3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInputH(inout AH2 r, inout AH2 g, inout AH2 b) {}

#else //CAS_FP16

AF3 CasLoad(ASU2 p)
{
    return io.InputTexture.Load(int3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b)
{
}

#endif //CAS_FP16

#include "ffx_cas.h"
#include "common.hlsli"

[numthreads(64, 1, 1)]
void main(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID)
{
    // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);

    bool sharpenOnly = true;
    
    io.InputTexture = ResourceDescriptorHeap[c_input];
    io.OutputTexture = ResourceDescriptorHeap[c_output];
    
#if CAS_FP16
    
    // Filter.
    AH4 c0, c1;
    AH2 cR, cG, cB;
    
    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c0.xyz), 1);
    io.OutputTexture[ASU2(gxy) + ASU2(8, 0)] = AF4(LinearToSrgb(c1.xyz), 1);
    gxy.y += 8u;
    
    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c0.xyz), 1);
    io.OutputTexture[ASU2(gxy) + ASU2(8, 0)] = AF4(LinearToSrgb(c1.xyz), 1);
    
#else
    
    // Filter.
    AF3 c;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c), 1);
    gxy.x += 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c), 1);
    gxy.y += 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c), 1);
    gxy.x -= 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    io.OutputTexture[ASU2(gxy)] = AF4(LinearToSrgb(c), 1);
    
#endif
}