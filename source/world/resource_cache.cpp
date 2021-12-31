#include "resource_cache.h"
#include "renderer/renderer.h"
#include "core/engine.h"

ResourceCache* ResourceCache::GetInstance()
{
    static ResourceCache cache;
    return &cache;
}

Texture2D* ResourceCache::GetTexture2D(const std::string& file, bool srgb)
{
    auto iter = m_cachedTexture2D.find(file);
    if (iter != m_cachedTexture2D.end())
    {
        iter->second.refCount++;
        return (Texture2D*)iter->second.ptr;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    Resource texture;
    texture.refCount = 1;
    texture.ptr = pRenderer->CreateTexture2D(file, srgb);
    m_cachedTexture2D.insert(std::make_pair(file, texture));

    return (Texture2D*)texture.ptr;
}

void ResourceCache::ReleaseTexture2D(Texture2D* texture)
{
    if (texture == nullptr)
    {
        return;
    }

    for (auto iter = m_cachedTexture2D.begin(); iter != m_cachedTexture2D.end(); ++iter)
    {
        if (iter->second.ptr == texture)
        {
            iter->second.refCount--;
            
            if (iter->second.refCount == 0)
            {
                delete texture;
                m_cachedTexture2D.erase(iter);
            }

            return;
        }
    }

    RE_ASSERT(false);
}
