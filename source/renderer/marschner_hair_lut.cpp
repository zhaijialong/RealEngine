#include "marschner_hair_lut.h"
#include "renderer.h"
#include "utils/gui_util.h"

//based on https://hairrendering.wordpress.com/2010/06/27/marschner-shader-part-ii/

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

MarschnerHairLUT::MarschnerHairLUT(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

void MarschnerHairLUT::Generate()
{
    eastl::vector<float4> M(textureWidth * textureHeight);

    for (uint32_t i = 0; i < textureWidth; ++i)
    {
        for (uint32_t j = 0; j < textureHeight; ++j)
        {
            float u = (i + 0.5f) / (float)textureWidth;
            float v = (j + 0.5f) / (float)textureHeight;

            float thetaI = asin(u * 2.0f - 1.0f);
            float thetaR = asin(v * 2.0f - 1.0f);
            float thetaH = (thetaI + thetaR) / 2.0f;
            float thetaD = (thetaI - thetaR) / 2.0f;

            M[i + j * textureWidth] = float4(
                g(betaR, radian_to_degree(thetaH) - alphaR),
                g(betaTT, radian_to_degree(thetaH) - alphaTT),
                g(betaTRT, radian_to_degree(thetaH) - alphaTRT),
                cos(thetaD));
        }
    }

    m_pM.reset(m_pRenderer->CreateTexture2D(textureWidth, textureHeight, 1, GfxFormat::RGBA32F, 0, "MarschnerHairLUT::M"));
    m_pRenderer->UploadTexture(m_pM->GetTexture(), &M[0]);
}

void MarschnerHairLUT::Debug()
{
    ImGui::Begin("Marschner M");
    ImGui::Image((ImTextureID)m_pM->GetSRV(), ImVec2((float)textureWidth, (float)textureHeight));
    ImGui::End();
}
