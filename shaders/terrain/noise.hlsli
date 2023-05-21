#pragma once

float2 grad(int2 z)  // replace this anything that returns a random vector
{
    // 2D to 1D  (feel free to replace by some other)
    int n = z.x + z.y * 11111;

    // Hugo Elias hash (feel free to replace by another one)
    n = (n << 13) ^ n;
    n = (n * (n * n * 15731 + 789221) + 1376312589) >> 16;

    // Perlin style vectors
    n &= 7;
    float2 gr = float2(n & 1, n >> 1) * 2.0 - 1.0;
    return (n >= 6) ? float2(0.0, gr.x) :
           (n >= 4) ? float2(gr.x, 0.0) :
                              gr;
}

// https://www.shadertoy.com/view/XdXBRH
// return gradient noise (in x) and its derivatives (in yz)
float3 noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    // quintic interpolation
    float2 u = f*f*f*(f*(f*6.0-15.0)+10.0);
    float2 du = 30.0*f*f*(f*(f-2.0)+1.0);

    float2 ga = grad(i + float2(0.0, 0.0));
    float2 gb = grad(i + float2(1.0, 0.0));
    float2 gc = grad(i + float2(0.0, 1.0));
    float2 gd = grad(i + float2(1.0, 1.0));
    
    float va = dot(ga, f - float2(0.0, 0.0));
    float vb = dot(gb, f - float2(1.0, 0.0));
    float vc = dot(gc, f - float2(0.0, 1.0));
    float vd = dot(gd, f - float2(1.0, 1.0));

    return float3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd), // value
                 ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) + // derivatives
                 du * (u.yx * (va - vb - vc + vd) + float2(vb, vc) - va));
}

// https://iquilezles.org/articles/fbm/
float fbm(float2 x)
{
    float f = 1.0;
    float a = 1.0;
    float t = 0.0;

    for (int i = 0; i < 12; i++)
    {
        t += a * noise(f * x).x;
        f *= 2.0;
        a *= 0.5;
    }
    return t;
}

float get_max_slope(float2 derivative)
{
    float max_slope_angle = atan(derivative.y / derivative.x);
    return abs(derivative.x * cos(max_slope_angle) + derivative.y * sin(max_slope_angle));
}

// https://www.shadertoy.com/view/NstfWs
//Sharpness should be a value between -1.0 (sharp) and 1.0 (smooth)
//  Determines how ridged vs regular vs billowy the noise is.
//
//Feature Amp should be a value between 0.0 and 1.0
//  Modifies the noise amplitude.
//Slope Erosion should be a value between 0.0 (no erosion) and 1.0 (eroded)
//  Determines how much the noise is eroded on the slopes.
//
//Altitude Erosion should be a value between 0.0 (no erosion) and 1.0 (eroded)
//  Determines how much the noise is eroded at low altitudes.
//
//Ridge Erosion should be a value between 0.0 (no erosion) and 1.0 (eroded)
//  Determines how much the noise is eroded due to sharpness.
float3 uber_noise(
    float2 p,
    int oct,
    float sharpness,
    float feature_amp,
    float slope_erosion,
    float altitude_erosion,
    float ridge_erosion,
    float lacunarity,
    float gain
    )
{
    float sum = 0.0, amp_sum = 0.0, amp = 1.0, damped_amp = 1.0;
    float2 slope_dsum = float2(0, 0), ridge_dsum = float2(0, 0);
    float2x2 m = float2x2(cos(0.5), -sin(0.5), sin(0.5), cos(0.5)) * lacunarity;
    
    for (int i = 0; i < oct; i++)
    {
        float3 n = noise(p);
        
        //Sharpness
        float feature = n.x;
        
        float ridged = 1.0 - abs(n.x);
        float billow = n.x * n.x;
        
        feature = lerp(feature, billow, max(0.0, sharpness));
        feature = lerp(feature, ridged, abs(min(0.0, sharpness)));
        
        feature *= feature_amp;
        
        ridge_dsum += -abs(n.yz) * amp * abs(min(0.0, sharpness));
        
        //Slope erosion
        slope_dsum += n.yz * damped_amp * slope_erosion;
        sum += feature * damped_amp / (1.0 + get_max_slope(slope_dsum));
        
        //Amplitude & Ridge erosion
        amp_sum += damped_amp;
        amp *= lerp(gain, gain * smoothstep(0.0, 1.0, 0.5 * sum + 0.5), altitude_erosion);
        damped_amp = amp * (1.0 - (ridge_erosion / (1.0 + dot(ridge_dsum, ridge_dsum))));
        
        p = mul(m, p);
    }
    
    return float3(sum, slope_dsum);
}