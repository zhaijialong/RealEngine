#pragma once

#include "renderer/renderer.h"

class BillboardSpriteRenderer
{
public:
    BillboardSpriteRenderer(Renderer* pRenderer);
    ~BillboardSpriteRenderer();

    void AddSprite(const float3& position, float size, Texture2D* texture, const float3& color);
    void Render();

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pSpritePSO = nullptr;

    struct Sprite
    {
        float3 position;
        float size;
        float3 color;
        uint32_t texture;
    };
    eastl::vector<Sprite> m_sprites;
};