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

    struct AliasedResource
    {
        IGfxResource* resource;
        bool isTexture = true;
        LifetimeRange lifetime;
        uint64_t lastUsedFrame = 0;
        GfxResourceState lastUsedState = GfxResourceState::Present;
    };

    struct Heap
    {
        IGfxHeap* heap;
        eastl::vector<AliasedResource> resources;

        bool IsOverlapping(const LifetimeRange& lifetime) const
        {
            for (size_t i = 0; i < resources.size(); ++i)
            {
                if (resources[i].lifetime.IsOverlapping(lifetime))
                {
                    return true;
                }
            }
            return false;
        }

        bool Contains(IGfxResource* resource) const
        {
            for (size_t i = 0; i < resources.size(); ++i)
            {
                if (resources[i].resource == resource)
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

    IGfxTexture* AllocateNonOverlappingTexture(const GfxTextureDesc& desc, const eastl::string& name, GfxResourceState& initial_state);
    void FreeNonOverlappingTexture(IGfxTexture* texture, GfxResourceState state);

    IGfxTexture* AllocateTexture(uint32_t firstPass, uint32_t lastPass, const GfxTextureDesc& desc, const eastl::string& name, GfxResourceState& initial_state);
    IGfxBuffer* AllocateBuffer(uint32_t firstPass, uint32_t lastPass, const GfxBufferDesc& desc, const eastl::string& name, GfxResourceState& initial_state);
    void Free(IGfxResource* resource, GfxResourceState state);

    IGfxResource* GetAliasedPrevResource(IGfxResource* resource, uint32_t firstPass);

    IGfxDescriptor* GetDescriptor(IGfxResource* resource, const GfxShaderResourceViewDesc& desc);
    IGfxDescriptor* GetDescriptor(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc);

private:
    void CheckHeapUsage(Heap& heap);
    void DeleteDescriptor(IGfxResource* resource);
    void AllocateHeap(uint32_t size);

private:
    IGfxDevice* m_pDevice;

    eastl::vector<Heap> m_allocatedHeaps;

    struct NonOverlappingTexture
    {
        IGfxTexture* texture;
        GfxResourceState lastUsedState;
        uint64_t lastUsedFrame;
    };
    eastl::vector<NonOverlappingTexture> m_freeOverlappingTextures;

    eastl::vector<SRVDescriptor> m_allocatedSRVs;
    eastl::vector<UAVDescriptor> m_allocatedUAVs;
};