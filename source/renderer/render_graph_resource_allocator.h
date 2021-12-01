#pragma once

#include "gfx/gfx.h"

class RenderGraphResourceAllocator
{
    struct LifetimeRange
    {
        uint32_t firstPass = UINT32_MAX;
        uint32_t lastPass = 0;

        void Reset() { firstPass = UINT32_MAX; lastPass = 0; }
        bool IsUsed() const { return firstPass != UINT32_MAX; }
        bool IsOverlapping(const LifetimeRange& other) const 
        { 
            if (IsUsed())
            {
                return firstPass <= other.lastPass && lastPass >= other.firstPass;
            }
            else
            {
                return false;
            }
        }
    };

    struct AliasedTexture
    {
        IGfxTexture* texture;
        LifetimeRange lifetime;
        uint64_t lastUsedFrame = 0;
        GfxResourceState lastUsedState = GfxResourceState::Present;
    };

    struct Heap
    {
        IGfxHeap* heap;
        std::vector<AliasedTexture> textures;

        bool IsOverlapping(const LifetimeRange& lifetime) const
        {
            for (size_t i = 0; i < textures.size(); ++i)
            {
                if (textures[i].lifetime.IsOverlapping(lifetime))
                {
                    return true;
                }
            }
            return false;
        }

        bool Contains(IGfxTexture* texture) const
        {
            for (size_t i = 0; i < textures.size(); ++i)
            {
                if (textures[i].texture == texture)
                {
                    return true;
                }
            }
            return false;
        }
    };

    struct SRVDescriptor
    {
        IGfxResource* resource;
        IGfxDescriptor* descriptor;
        GfxShaderResourceViewDesc desc;
    };

    struct UAVDescriptor
    {
        IGfxResource* resource;
        IGfxDescriptor* descriptor;
        GfxUnorderedAccessViewDesc desc;
    };

public:
    RenderGraphResourceAllocator(IGfxDevice* pDevice);
    ~RenderGraphResourceAllocator();

    void Reset();

    IGfxTexture* AllocateNonOverlappingTexture(const GfxTextureDesc& desc, const std::string& name, GfxResourceState& initial_state);
    void FreeNonOverlappingTexture(IGfxTexture* texture, GfxResourceState state);

    IGfxTexture* AllocateTexture(uint32_t firstPass, uint32_t lastPass, const GfxTextureDesc& desc, const std::string& name, GfxResourceState& initial_state);
    void Free(IGfxTexture* texture, GfxResourceState state);

    IGfxTexture* GetAliasedPrevResource(IGfxTexture* texture, uint32_t firstPass);

    IGfxDescriptor* GetDescriptor(IGfxResource* resource, const GfxShaderResourceViewDesc& desc);
    IGfxDescriptor* GetDescriptor(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc);

private:
    void CheckHeapUsage(Heap& heap);
    void DeleteDescriptor(IGfxResource* resource);

private:
    IGfxDevice* m_pDevice;

    std::vector<Heap> m_allocatedHeaps;

    struct NonOverlappingTexture
    {
        IGfxTexture* texture;
        GfxResourceState lastUsedState;
        uint64_t lastUsedFrame;
    };
    std::vector<NonOverlappingTexture> m_freeOverlappingTextures;

    std::vector<SRVDescriptor> m_allocatedSRVs;
    std::vector<UAVDescriptor> m_allocatedUAVs;
};