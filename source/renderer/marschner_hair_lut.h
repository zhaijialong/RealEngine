#pragma once

#include "resource/texture_2d.h"
#include "resource/texture_3d.h"

class Renderer;

class MarschnerHairLUT
{
public:
    MarschnerHairLUT(Renderer* pRenderer);
    
    void Generate();
    void Debug();

    Texture3D* GetM() const { return m_pM.get(); }
    Texture2D* GetN() const { return m_pN.get(); }

private:
    void GenerateM();
    void GenerateN();

private:
    Renderer* m_pRenderer;

    eastl::unique_ptr<Texture3D> m_pM;
    eastl::unique_ptr<Texture2D> m_pN;
};