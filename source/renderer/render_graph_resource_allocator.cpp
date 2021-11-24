#include "render_graph_resource_allocator.h"
#include "utils/assert.h"

RenderGraphResourceAllocator::RenderGraphResourceAllocator(IGfxDevice* pDevice)
{
    m_pDevice = pDevice;
}

RenderGraphResourceAllocator::~RenderGraphResourceAllocator()
{
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end(); ++iter)
    {
        const Heap& heap = *iter;

        for (size_t i = 0; i < heap.textures.size(); ++i)
        {
            DeleteDescriptor(heap.textures[i].texture);
            delete heap.textures[i].texture;
        }

        delete heap.heap;
    }
}

void RenderGraphResourceAllocator::Reset()
{
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end();)
    {
        Heap& heap = *iter;
        CheckHeapUsage(heap);

        if (heap.textures.empty())
        {
            delete heap.heap;
            iter = m_allocatedHeaps.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

void RenderGraphResourceAllocator::CheckHeapUsage(Heap& heap)
{
    uint64_t current_frame = m_pDevice->GetFrameID();

    for (auto iter = heap.textures.begin(); iter != heap.textures.end();)
    {
        const AliasedTexture aliasedTexture = *iter;

        if (current_frame - aliasedTexture.lastUsedFrame > 30)
        {
            DeleteDescriptor(aliasedTexture.texture);

            delete aliasedTexture.texture;
            iter = heap.textures.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

IGfxTexture* RenderGraphResourceAllocator::AllocateTexture(uint32_t firstPass, uint32_t lastPass, const GfxTextureDesc& desc, const std::string& name, GfxResourceState& initial_state)
{
    LifetimeRange lifetime = { firstPass, lastPass };
    uint32_t texture_size = m_pDevice->GetAllocationSize(desc);

    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        Heap& heap = m_allocatedHeaps[i];
        if (heap.heap->GetDesc().size < texture_size ||
            heap.IsOverlapping(lifetime))
        {
            continue;
        }

        for (size_t j = 0; j < heap.textures.size(); ++j)
        {
            AliasedTexture& aliasedTexture = heap.textures[j];
            if (!aliasedTexture.lifetime.IsUsed() && aliasedTexture.texture->GetDesc() == desc)
            {
                aliasedTexture.lifetime = lifetime;
                initial_state = aliasedTexture.lastUsedState;
                return aliasedTexture.texture;
            }
        }        

        AliasedTexture aliasedTexture;
        aliasedTexture.texture = m_pDevice->CreateTexture(desc, heap.heap, 0, "RGTexture " + name);
        aliasedTexture.lifetime = lifetime;
        heap.textures.push_back(aliasedTexture);

        if (IsDepthFormat(desc.format))
        {
            initial_state = GfxResourceState::DepthStencil;
        }
        else if (desc.usage & GfxTextureUsageRenderTarget)
        {
            initial_state = GfxResourceState::RenderTarget;
        }
        else if (desc.usage & GfxTextureUsageUnorderedAccess)
        {
            initial_state = GfxResourceState::UnorderedAccess;
        }

        RE_ASSERT(aliasedTexture.texture != nullptr);
        return aliasedTexture.texture;
    }

    GfxHeapDesc heapDesc;
    heapDesc.size = texture_size;

    char heapName[32];
    std::sprintf(heapName, "RG Heap %.1f MB", heapDesc.size / (1024.0f * 1024.0f));

    Heap heap;
    heap.heap = m_pDevice->CreateHeap(heapDesc, heapName);
    m_allocatedHeaps.push_back(heap);

    return AllocateTexture(firstPass, lastPass, desc, name, initial_state);
}

void RenderGraphResourceAllocator::Free(IGfxTexture* texture, GfxResourceState state)
{
    if (texture != nullptr)
    {
        for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
        {
            Heap& heap = m_allocatedHeaps[i];

            for (size_t j = 0; j < heap.textures.size(); ++j)
            {
                AliasedTexture& aliasedTexture = heap.textures[j];
                if (aliasedTexture.texture == texture)
                {
                    aliasedTexture.lifetime.Reset();
                    aliasedTexture.lastUsedFrame = m_pDevice->GetFrameID();
                    aliasedTexture.lastUsedState = state;

                    return;
                }
            }
        }

        RE_ASSERT(false);
    }
}

IGfxTexture* RenderGraphResourceAllocator::GetAliasedPrevResource(IGfxTexture* texture, uint32_t firstPass)
{
    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        const Heap& heap = m_allocatedHeaps[i];
        if (!heap.Contains(texture))
        {
            continue;
        }

        IGfxTexture* prev_texture = nullptr;
        uint32_t prev_texture_lastpass = 0;

        for (size_t j = 0; j < heap.textures.size(); ++j)
        {
            const AliasedTexture& aliasedTexture = heap.textures[j];

            if (aliasedTexture.texture != texture &&
                aliasedTexture.lifetime.lastPass < firstPass &&
                aliasedTexture.lifetime.lastPass > prev_texture_lastpass)
            {
                prev_texture = aliasedTexture.texture;
                prev_texture_lastpass = aliasedTexture.lifetime.lastPass;
            }
        }

        return prev_texture;
    }

    RE_ASSERT(false);
    return nullptr;
}

IGfxDescriptor* RenderGraphResourceAllocator::GetDescriptor(IGfxResource* resource, const GfxShaderResourceViewDesc& desc)
{
    for (size_t i = 0; i < m_allocatedSRVs.size(); ++i)
    {
        if (m_allocatedSRVs[i].resource == resource &&
            m_allocatedSRVs[i].desc == desc)
        {
            return m_allocatedSRVs[i].descriptor;
        }
    }

    IGfxDescriptor* srv = m_pDevice->CreateShaderResourceView(resource, desc, resource->GetName());
    m_allocatedSRVs.push_back({ resource, srv, desc });

    return srv;
}

IGfxDescriptor* RenderGraphResourceAllocator::GetDescriptor(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc)
{
    for (size_t i = 0; i < m_allocatedUAVs.size(); ++i)
    {
        if (m_allocatedUAVs[i].resource == resource &&
            m_allocatedUAVs[i].desc == desc)
        {
            return m_allocatedUAVs[i].descriptor;
        }
    }

    IGfxDescriptor* srv = m_pDevice->CreateUnorderedAccessView(resource, desc, resource->GetName());
    m_allocatedUAVs.push_back({ resource, srv, desc });

    return srv;
}

void RenderGraphResourceAllocator::DeleteDescriptor(IGfxResource* resource)
{
    for (auto iter = m_allocatedSRVs.begin(); iter != m_allocatedSRVs.end(); )
    {
        if (iter->resource == resource)
        {
            delete iter->descriptor;
            iter = m_allocatedSRVs.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    for (auto iter = m_allocatedUAVs.begin(); iter != m_allocatedUAVs.end(); )
    {
        if (iter->resource == resource)
        {
            delete iter->descriptor;
            iter = m_allocatedUAVs.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}
