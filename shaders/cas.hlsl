cbuffer cb : register(b1)
{
    uint c_input;
    uint c_output;
    uint2 __padding;
    
    uint4 const0;
    uint4 const1;
};

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
    Texture2D inputTexture = ResourceDescriptorHeap[c_input];
    return (AH3)inputTexture.Load(ASU3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInputH(inout AH2 r, inout AH2 g, inout AH2 b) {}

#else //CAS_FP16

AF3 CasLoad(ASU2 p)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_input];
    return inputTexture.Load(int3(p, 0)).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b)
{
}

#endif //CAS_FP16

#include "ffx_cas.h"
#include "common.hlsli"

void CasStore(int2 p, float3 c)
{
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_output];
#if GFX_BACKEND_METAL
    outputTexture[p] = float4(c, 1.0);
#else
    outputTexture[p] = float4(LinearToSrgb(c), 1.0);
#endif
}

[numthreads(64, 1, 1)]
void main(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID)
{
    // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);

    bool sharpenOnly = true;
    
#if CAS_FP16
    
    // Filter.
    AH4 c0, c1;
    AH2 cR, cG, cB;
    
    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    CasStore(ASU2(gxy), c0.xyz);
    CasStore(ASU2(gxy) + ASU2(8, 0), c1.xyz);
    gxy.y += 8u;
    
    CasFilterH(cR, cG, cB, gxy, const0, const1, sharpenOnly);
    CasDepack(c0, c1, cR, cG, cB);
    CasStore(ASU2(gxy), c0.xyz);
    CasStore(ASU2(gxy) + ASU2(8, 0), c1.xyz);
    
#else
    
    // Filter.
    AF3 c;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    CasStore(ASU2(gxy), c);
    gxy.x += 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    CasStore(ASU2(gxy), c);
    gxy.y += 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    CasStore(ASU2(gxy), c);
    gxy.x -= 8u;
    
    CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);
    CasStore(ASU2(gxy), c);
    
#endif
}