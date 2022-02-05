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

uint32_t ResourceCache::GetSceneBuffer(const std::string& name, const void* data, uint32_t size)
{
    auto iter = m_cachedSceneBuffer.find(name);
    if (iter != m_cachedSceneBuffer.end())
    {
        iter->second.refCount++;
        return iter->second.address;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    SceneBuffer buffer;
    buffer.refCount = 1;
    buffer.address = pRenderer->AllocateSceneStaticBuffer(data, size);
    m_cachedSceneBuffer.insert(std::make_pair(name, buffer));

    return buffer.address;
}

void ResourceCache::RelaseSceneBuffer(uint32_t address)
{
    if (address == -1)
    {
        return;
    }

    for (auto iter = m_cachedSceneBuffer.begin(); iter != m_cachedSceneBuffer.end(); ++iter)
    {
        if (iter->second.address == address)
        {
            iter->second.refCount--;

            if (iter->second.refCount == 0)
            {
                Engine::GetInstance()->GetRenderer()->FreeSceneStaticBuffer(address);
                m_cachedSceneBuffer.erase(iter);
            }

            return;
        }
    }

    RE_ASSERT(false);
}
