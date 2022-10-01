#include "marschner_hair_lut.h"
#include "renderer.h"
#include "utils/gui_util.h"

// reference:
// Marschner et al. 2003, "Light Scattering from Human Hair Fibers"
// https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-23-hair-animation-and-rendering-nalu-demo
// https://hairrendering.wordpress.com/2010/06/27/marschner-shader-part-ii/

static const uint32_t textureWidth = 128;
static const uint32_t textureHeight = 128;

static const float alphaR = radian_to_degree(-0.07f); //same as UE's shift 0.035
static const float alphaTT = -alphaR / 2.0f;
static const float alphaTRT = -3.0f * alphaR / 2.0f;

static const float betaR = 5; //todo : lerp(5, 10, roughness)
static const float betaTT = betaR / 2.0f;
static const float betaTRT = betaR * 2.0f;

static const float indexOfRefraction = 1.55f;
static const float absorption = 0.2f;

// https://en.wikipedia.org/wiki/Normal_distribution
float NormalDistribution(float sigma, float x_mu)
{
    return expf(-0.5f * x_mu * x_mu / (sigma * sigma)) / (sqrtf(2.0f * M_PI) * sigma);
}

// https://en.wikipedia.org/wiki/Cubic_equation
// a * x^3 + b * x^2 + c * x + d = 0
struct CubicEquationSolver
{
    float roots[3] = {};
    uint32_t rootCount = 0;

    void Solve(float a, float b, float c, float d)
    {
        RE_ASSERT(b == 0.0f); //b should be always 0 for our case

        if (abs(a) < FLT_EPSILON)
        {
            // linear equation
            if (abs(c) > FLT_EPSILON)
            {
                rootCount = 1;
                roots[0] = -d / c;
            }
        }
        else
        {
            //normalized to Cardano's formula: x^3 + p * x = q
            float p = c / a;
            float q = -d / a;

            if (abs(q) < FLT_EPSILON)
            {
                roots[0] = 0.0f;
                rootCount = 1;

                if (-p > 0.0)
                {
                    roots[1] = sqrtf(-p);
                    roots[2] = -sqrtf(-p);
                    rootCount = 3;
                }
            }
            else
            {
                float Q = p / 3.0f;
                float R = q / 2.0f;
                float D = Q * Q * Q + R * R;

                if (D > 0.0f)
                {
                    float sqrtD = sqrtf(D);
                    float x = sign(R + sqrtD);
                    float S = sign(R + sqrtD) * powf(abs(R + sqrtD), 1.0f / 3.0f);
                    float T = sign(R - sqrtD) * powf(abs(R - sqrtD), 1.0f / 3.0f);

                    roots[0] = S + T;
                    rootCount = 1;
                }
                else
                {
                    float theta = acos(R / sqrtf(-Q * Q * Q));
                    float sqrtQ = sqrtf(-Q);

                    roots[0] = 2.0f * sqrtQ * cos(theta / 3.0f);
                    roots[1] = 2.0f * sqrtQ * cos((theta + 2.0f * M_PI) / 3.0f);
                    roots[2] = 2.0f * sqrtQ * cos((theta + 4.0f * M_PI) / 3.0f);

                    rootCount = 3;
                }
            }
        }
    }
};

// Marschner et al. 2003, "Light Scattering from Human Hair Fibers", Appendix B
float PerpendicularIOR(float ior, float theta)
{
    return sqrtf(ior * ior - sin(theta) * sin(theta)) / cos(theta);
}

// Marschner et al. 2003, "Light Scattering from Human Hair Fibers", Appendix B
float ParallelIOR(float ior, float theta)
{
    return ior * ior * cos(theta) / sqrtf(ior * ior - sin(theta) * sin(theta));
}

// https://en.wikipedia.org/wiki/Total_internal_reflection
bool TotalInternalReflection(float n1, float n2, float gammaI)
{
    if (n1 > n2)
    {
        if (gammaI > asin(n2 / n1))
        {
            return true;
        }
    }
    return false;
}

float FresnelPerpendicular(float ior, float gammaI)
{
    float n1 = 1.0f;
    float n2 = ior;
    float gammaT = asin(clamp(sin(gammaI) / ior, -1, 1));

    if (TotalInternalReflection(n1, n2, gammaI))
    {
        return 1.0f;
    }

    float Rs = (n1 * cos(gammaI) - n2 * cos(gammaT)) /
        (n1 * cos(gammaI) + n2 * cos(gammaT));

    return Rs * Rs;
}

float FresnelParallel(float ior, float gammaI)
{
    float n1 = 1.0f;
    float n2 = ior;
    float gammaT = asin(clamp(sin(gammaI) / ior, -1, 1));

    if (TotalInternalReflection(n1, n2, gammaI))
    {
        return 1.0f;
    }

    float Rp = (n2 * cos(gammaI) - n1 * cos(gammaT)) /
        (n2 * cos(gammaI) + n1 * cos(gammaT));

    return Rp * Rp;
}

// https://en.wikipedia.org/wiki/Fresnel_equations
float Fresnel(float perpendicularIOR, float parallelIOR, float gamma)
{
    return (FresnelPerpendicular(perpendicularIOR, gamma) + FresnelParallel(parallelIOR, gamma)) / 2.0f;
}

float A(uint32_t p, float h, float perpendicularIOR, float parallelIOR)
{
    float gammaI = asin(h);
    float gammaT = asin(h / perpendicularIOR);
    float F = Fresnel(perpendicularIOR, parallelIOR, gammaI);

    if (p == 0)
    {
        return F;
    }

    float invF = Fresnel(1.0f / perpendicularIOR, 1.0f / parallelIOR, gammaT);
    float T = exp(-2.0f * absorption * (1.0f + cos(2.0f * gammaT)));

    return (1.0f - F) * (1.0f - F) * pow(invF, float(p - 1)) * pow(T, (float)p);
}

// phi is Equation 10, h = sin(gammaI)
float DPhiDH(uint32_t p, float c, float h)
{
    float gammaI = asin(h);
    float dPhiDGammaI = (6.0f * p * c / M_PI - 2.0f) - 3.0f * 8.0f * p * c * gammaI * gammaI / (M_PI * M_PI * M_PI);
    float dGammaIDH = 1.0f / sqrtf(1.0f - h * h); //derivative of asin

    return dPhiDGammaI * dGammaIDH;
}

float Np(uint32_t p, float phi, float thetaD)
{
    float perpendicularIOR = PerpendicularIOR(indexOfRefraction, thetaD);
    float parallelIOR = ParallelIOR(indexOfRefraction, thetaD);

    CubicEquationSolver cubicSolver;

    //solve incident angles, Marschner's paper Equation 10
    float c = asin(1.0f / perpendicularIOR);
    cubicSolver.Solve(
        -8.0f * p * c / (M_PI * M_PI * M_PI),
        0.0f,
        6.0f * p * c / M_PI - 2.0f,
        p * M_PI - phi
    );

    float N = 0.0f;
    for (uint32_t i = 0; i < cubicSolver.rootCount; ++i)
    {
        float h = sin(cubicSolver.roots[i]); //Figure 9

        N += A(p, h, perpendicularIOR, parallelIOR) / abs(2.0f * DPhiDH(p, c, h)); //Equation 8
    }

    return N;
}

MarschnerHairLUT::MarschnerHairLUT(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

void MarschnerHairLUT::Generate()
{
    GenerateM();
    GenerateN();
}

void MarschnerHairLUT::Debug()
{
    ImGui::Begin("Marschner M");
    ImGui::Image((ImTextureID)m_pM->GetSRV(), ImVec2((float)textureWidth, (float)textureHeight));
    ImGui::End();

    ImGui::Begin("Marschner N");
    ImGui::Image((ImTextureID)m_pN->GetSRV(), ImVec2((float)textureWidth, (float)textureHeight));
    ImGui::End();
}

void MarschnerHairLUT::GenerateM()
{
    eastl::vector<ushort4> M(textureWidth * textureHeight);

    for (uint32_t i = 0; i < textureWidth; ++i)
    {
        for (uint32_t j = 0; j < textureHeight; ++j)
        {
            float u = (i + 0.5f) / (float)textureWidth; //dot(T, L) * 0.5 + 0.5
            float v = (j + 0.5f) / (float)textureHeight;//dot(T, V) * 0.5 + 0.5

            float thetaI = asin(u * 2.0f - 1.0f); //[-PI/2, PI/2]
            float thetaR = asin(v * 2.0f - 1.0f); //[-PI/2, PI/2]
            float thetaH = (thetaI + thetaR) / 2.0f; //[-PI/2, PI/2]
            float thetaD = (thetaI - thetaR) / 2.0f; //[-PI/2, PI/2]

            float4 value = float4(
                NormalDistribution(betaR, radian_to_degree(thetaH) - alphaR),
                NormalDistribution(betaTT, radian_to_degree(thetaH) - alphaTT),
                NormalDistribution(betaTRT, radian_to_degree(thetaH) - alphaTRT),
                cos(thetaD)); //[0, 1]

            M[i + j * textureWidth] = ushort4(
                FloatToHalf(value.x),
                FloatToHalf(value.y),
                FloatToHalf(value.z),
                FloatToHalf(value.w)
            );
        }
    }

    m_pM.reset(m_pRenderer->CreateTexture2D(textureWidth, textureHeight, 1, GfxFormat::RGBA16F, 0, "MarschnerHairLUT::M"));
    m_pRenderer->UploadTexture(m_pM->GetTexture(), &M[0]);
}

void MarschnerHairLUT::GenerateN()
{
    eastl::vector<ushort4> N(textureWidth * textureHeight);

    for (uint32_t i = 0; i < textureWidth; ++i)
    {
        for (uint32_t j = 0; j < textureHeight; ++j)
        {
            float u = (i + 0.5f) / (float)textureWidth; //cos(phi) * 0.5 + 0.5
            float v = (j + 0.5f) / (float)textureHeight;//cos(thetaD), in [0, 1]

            float phi = acos(u * 2.0f - 1.0f);
            float thetaD = acos(v);

            float4 value = float4(
                Np(0, phi, thetaD),
                Np(1, phi, thetaD),
                Np(2, phi, thetaD),
                1.0f);

            N[i + j * textureWidth] = ushort4(
                FloatToHalf(value.x),
                FloatToHalf(value.y),
                FloatToHalf(value.z),
                FloatToHalf(value.w)
            );
        }
    }

    m_pN.reset(m_pRenderer->CreateTexture2D(textureWidth, textureHeight, 1, GfxFormat::RGBA16F, 0, "MarschnerHairLUT::N"));
    m_pRenderer->UploadTexture(m_pN->GetTexture(), &N[0]);
}
