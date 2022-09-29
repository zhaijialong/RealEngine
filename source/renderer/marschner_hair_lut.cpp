#include "marschner_hair_lut.h"
#include "renderer.h"
#include "utils/gui_util.h"

// reference:
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

//https://en.wikipedia.org/wiki/Normal_distribution
float g(float sigma, float x_mu)
{
    return expf(-0.5f * x_mu * x_mu / (sigma * sigma)) / (sqrtf(2.0f * M_PI) * sigma);
}

float Np(uint32_t p, float phi, float thetaD)
{
    return 1.0f;
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
                g(betaR, radian_to_degree(thetaH) - alphaR),
                g(betaTT, radian_to_degree(thetaH) - alphaTT),
                g(betaTRT, radian_to_degree(thetaH) - alphaTRT),
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
