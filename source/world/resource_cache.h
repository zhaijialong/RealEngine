#pragma once

#include "renderer/resource/texture_2d.h"
#include <unordered_map>

class ResourceCache
{
public:
    static ResourceCache* GetInstance();

    Texture2D* GetTexture2D(const std::string& file, bool srgb = true);
    void ReleaseTexture2D(Texture2D* texture);

    uint32_t GetSceneBuffer(const std::string& name, void* data, uint32_t size);
    void RelaseSceneBuffer(uint32_t address);

private:
    struct Resource
    {
        void* ptr;
        uint32_t refCount;
    };

    struct SceneBuffer
    {
        uint32_t address;
        uint32_t refCount;
    };

    std::unordered_map<std::string, Resource> m_cachedTexture2D;
    std::unordered_map<std::string, SceneBuffer> m_cachedSceneBuffer;
};