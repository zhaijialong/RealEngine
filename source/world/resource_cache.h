#pragma once

#include "renderer/renderer.h"
#include "EASTL/hash_map.h"

class ResourceCache
{
public:
    static ResourceCache* GetInstance();

    Texture2D* GetTexture2D(const eastl::string& file, bool srgb = true);
    void ReleaseTexture2D(Texture2D* texture);

    OffsetAllocator::Allocation GetSceneBuffer(const eastl::string& name, const void* data, uint32_t size);
    void RelaseSceneBuffer(OffsetAllocator::Allocation allocation);

private:
    struct Resource
    {
        void* ptr;
        uint32_t refCount;
    };

    struct SceneBuffer
    {
        OffsetAllocator::Allocation allocation;
        uint32_t refCount;
    };

    eastl::hash_map<eastl::string, Resource> m_cachedTexture2D;
    eastl::hash_map<eastl::string, SceneBuffer> m_cachedSceneBuffer;
};