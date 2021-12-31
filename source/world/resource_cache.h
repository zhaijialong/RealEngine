#pragma once

#include "renderer/resource/texture_2d.h"
#include <unordered_map>

class ResourceCache
{
public:
    static ResourceCache* GetInstance();

    Texture2D* GetTexture2D(const std::string& file, bool srgb = true);
    void ReleaseTexture2D(Texture2D* texture);

private:
    struct Resource
    {
        void* ptr;
        uint32_t refCount;
    };

    std::unordered_map<std::string, Resource> m_cachedTexture2D;
};