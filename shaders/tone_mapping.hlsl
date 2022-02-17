#include "common.hlsli"
#include "exposure.hlsli"

cbuffer CB : register(b0)
{
    uint c_hdrTexture;
    uint c_ldrTexture;
    uint c_avgLuminanceTexture;
    uint c_avgLuminanceMip;
};

//////////////////////////////////////////////////////////////////////////////////////////
//https://github.com/EmbarkStudios/kajiya/blob/main/assets/shaders/inc/tonemap.hlsl
float tonemap_curve(float v)
{
    return 1.0 - exp(-v);
}

float3 tonemap_curve(float3 v)
{
    return float3(tonemap_curve(v.r), tonemap_curve(v.g), tonemap_curve(v.b));
}

float3 neutral_tonemap(float3 col)
{
    float3 ycbcr = RGBToYCbCr(col);

    float bt = tonemap_curve(length(ycbcr.yz) * 2.4);
    float desat = max((bt - 0.7) * 0.8, 0.0);
    desat *= desat;

    float3 desat_col = lerp(col.rgb, ycbcr.xxx, desat);

    float tm_luma = tonemap_curve(ycbcr.x);
    float3 tm0 = col.rgb * max(0.0, tm_luma / max(1e-5, Luminance(col.rgb)));
    float final_mult = 0.97;
    float3 tm1 = tonemap_curve(desat_col);

    col = lerp(tm0, tm1, bt * bt);

    return col * final_mult;
}
//////////////////////////////////////////////////////////////////////////////////////////

float3 ApplyExposure(float3 color)
{
    // 683 : luminance/radiance conversion
    // http://www.dfisica.ubi.pt/~hgil/Fotometria/HandBook/ch07.html

#if AUTO_EXPOSURE
    Texture2D<float> avgLuminanceTexture = ResourceDescriptorHeap[c_avgLuminanceTexture];
    float avgLuminance = avgLuminanceTexture.Load(uint3(0, 0, c_avgLuminanceMip)) * 683;

    float EV100 = ComputeEV100(avgLuminance );
#else
    float EV100 = ComputeEV100(CameraCB.physicalCamera.aperture, CameraCB.physicalCamera.shutterSpeed, CameraCB.physicalCamera.iso);
#endif

    EV100 -= CameraCB.physicalCamera.exposureCompensation;
    float exposure = ConvertEV100ToExposure(EV100);

    return color * exposure * 683;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    Texture2D hdrTexture = ResourceDescriptorHeap[c_hdrTexture];    
    float3 color = hdrTexture[dispatchThreadID.xy].xyz;

    color = ApplyExposure(color);
    color = neutral_tonemap(color);   
    
    RWTexture2D<float4> ldrTexture = ResourceDescriptorHeap[c_ldrTexture];
    ldrTexture[dispatchThreadID.xy] = float4(LinearToSrgb(color.xyz), 1.0);
}