#include "light.h"
#include "resource_cache.h"
#include "billboard_sprite.h"
#include "core/engine.h"

ILight::~ILight()
{
    ResourceCache::GetInstance()->ReleaseTexture2D(m_pIconTexture);
}

void ILight::Render(Renderer* pRenderer)
{
    if (m_pIconTexture)
    {
        World* world = Engine::GetInstance()->GetWorld();
        world->GetBillboardSpriteRenderer()->AddSprite(m_pos, 32, m_pIconTexture, float4(1.0f, 1.0f, 1.0f, 1.0f), m_nID);
    }
}
