#include "rect_light.h"
#include "resource_cache.h"
#include "core/engine.h"

bool RectLight::Create()
{
    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pIconTexture = ResourceCache::GetInstance()->GetTexture2D(asset_path + "ui/rect_light.png");

    return true;
}

void RectLight::Tick(float delta_time)
{
}
