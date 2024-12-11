#pragma once

#include "renderer/renderer.h"

class BillboardSpriteRenderer
{
public:
    BillboardSpriteRenderer(Renderer* pRenderer);
    ~BillboardSpriteRenderer();

    void AddSprite(const float3& position, float size, Texture2D* texture, const float4& color, uint32_t objectID);
    void Render();

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pSpritePSO = nullptr;
    IGfxPipelineState* m_pSpriteObjectIDPSO = nullptr;

    struct Sprite
    {
        float3 position;
        float size;

        uint32_t color;
        uint32_t texture;
        uint32_t objectID;
        uint32_t _pad;
    };
    eastl::vector<Sprite> m_sprites;
};