cbuffer CB : register(b0)
{
    uint c_seed;
    uint c_outputTexture;
}

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

// https://www.shadertoy.com/view/XdXGW8
float noise(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
	
    float2 u = f * f * (3.0 - 2.0 * f); // feel free to replace by a quintic smoothstep instead

    return lerp(lerp(dot(grad(i + int2(0, 0)), f - float2(0.0, 0.0)),
                     dot(grad(i + int2(1, 0)), f - float2(1.0, 0.0)), u.x),
                lerp(dot(grad(i + int2(0, 1)), f - float2(0.0, 1.0)),
                     dot(grad(i + int2(1, 1)), f - float2(1.0, 1.0)), u.x), u.y);
}

// https://iquilezles.org/articles/fbm/
float fbm(float2 x)
{
    float f = 1.0;
    float a = 1.0;
    float t = 0.0;

    for (int i = 0; i < 10; i++)
    {
        t += a * noise(f * x);
        f *= 2.0;
        a *= 0.5;
    }
    return t;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{    
    RWTexture2D<float> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    
    float width, height;
    outputTexture.GetDimensions(width, height);

    float2 uv = (dispatchThreadID.xy + 0.5) / float2(width, height);
    uv += float2(fmod(c_seed, 43.5), fmod(c_seed, 63.7));
    
    float f = fbm(uv) * 0.5 + 0.5;
    f = pow(f, 3.0);
    
    outputTexture[dispatchThreadID.xy] = f;
}