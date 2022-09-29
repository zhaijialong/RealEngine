#pragma once

#include "resource/texture_2d.h"

class Renderer;

class MarschnerHairLUT
{
public:
    MarschnerHairLUT(Renderer* pRenderer);
    
    void Generate();
    void Debug();

private:
    void GenerateM();
    void GenerateN();

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture2D> m_pM;
    eastl::unique_ptr<Texture2D> m_pN;
};