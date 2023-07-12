#include "render_graph_resource_allocator.h"
#include "utils/math.h"
#include "utils/fmt.h"

RenderGraphResourceAllocator::RenderGraphResourceAllocator(IGfxDevice* pDevice)
{
    m_pDevice = pDevice;
}

RenderGraphResourceAllocator::~RenderGraphResourceAllocator()
{
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end(); ++iter)
    {
        const Heap& heap = *iter;

        for (size_t i = 0; i < heap.resources.size(); ++i)
        {
            DeleteDescriptor(heap.resources[i].resource);
            delete heap.resources[i].resource;
        }

        delete heap.heap;
    }

    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); ++iter)
    {
        DeleteDescriptor(iter->texture);
        delete iter->texture;
    }
}

void RenderGraphResourceAllocator::Reset()
{
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end();)
    {
        Heap& heap = *iter;
        CheckHeapUsage(heap);

        if (heap.resources.empty())
        {
            delete heap.heap;
            iter = m_allocatedHeaps.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    uint64_t current_frame = m_pDevice->GetFrameID();

    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); )
    {
        if (current_frame - iter->lastUsedFrame > 30)
        {
            DeleteDescriptor(iter->texture);
            delete iter->texture;
            iter = m_freeOverlappingTextures.erase(iter);
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

    for (auto iter = heap.resources.begin(); iter != heap.resources.end();)
    {
        const AliasedResource aliasedResource = *iter;

        if (current_frame - aliasedResource.lastUsedFrame > 30)
        {
            DeleteDescriptor(aliasedResource.resource);

            delete aliasedResource.resource;
            iter = heap.resources.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

IGfxTexture* RenderGraphResourceAllocator::AllocateTexture(uint32_t firstPass, uint32_t lastPass, const GfxTextureDesc& desc, const eastl::string& name, GfxAccessFlags& initial_state)
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

        for (size_t j = 0; j < heap.resources.size(); ++j)
        {
            AliasedResource& aliasedResource = heap.resources[j];
            if (aliasedResource.isTexture && !aliasedResource.lifetime.IsUsed() && ((IGfxTexture*)aliasedResource.resource)->GetDesc() == desc)
            {
                aliasedResource.lifetime = lifetime;
                initial_state = aliasedResource.lastUsedState;
                return (IGfxTexture*)aliasedResource.resource;
            }
        }        

        GfxTextureDesc newDesc = desc;
        newDesc.heap = heap.heap;

        AliasedResource aliasedTexture;
        aliasedTexture.resource = m_pDevice->CreateTexture(newDesc, "RGTexture " + name);
        aliasedTexture.isTexture = true;
        aliasedTexture.lifetime = lifetime;
        heap.resources.push_back(aliasedTexture);

        if (IsDepthFormat(desc.format))
        {
            initial_state = GfxAccessDSV;
        }
        else if (desc.usage & GfxTextureUsageRenderTarget)
        {
            initial_state = GfxAccessRTV;
        }
        else if (desc.usage & GfxTextureUsageUnorderedAccess)
        {
            initial_state = GfxAccessMaskUAV;
        }

        RE_ASSERT(aliasedTexture.resource != nullptr);
        return (IGfxTexture*)aliasedTexture.resource;
    }

    AllocateHeap(texture_size);
    return AllocateTexture(firstPass, lastPass, desc, name, initial_state);
}

IGfxBuffer* RenderGraphResourceAllocator::AllocateBuffer(uint32_t firstPass, uint32_t lastPass, const GfxBufferDesc& desc, const eastl::string& name, GfxAccessFlags& initial_state)
{
    LifetimeRange lifetime = { firstPass, lastPass };
    uint32_t buffer_size = desc.size;

    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        Heap& heap = m_allocatedHeaps[i];
        if (heap.heap->GetDesc().size < buffer_size ||
            heap.IsOverlapping(lifetime))
        {
            continue;
        }

        for (size_t j = 0; j < heap.resources.size(); ++j)
        {
            AliasedResource& aliasedResource = heap.resources[j];
            if (!aliasedResource.isTexture && !aliasedResource.lifetime.IsUsed() && ((IGfxBuffer*)aliasedResource.resource)->GetDesc() == desc)
            {
                aliasedResource.lifetime = lifetime;
                initial_state = aliasedResource.lastUsedState;
                return (IGfxBuffer*)aliasedResource.resource;
            }
        }

        GfxBufferDesc newDesc = desc;
        newDesc.heap = heap.heap;

        AliasedResource aliasedBuffer;
        aliasedBuffer.resource = m_pDevice->CreateBuffer(newDesc, "RGBuffer " + name);
        aliasedBuffer.isTexture = false;
        aliasedBuffer.lifetime = lifetime;
        heap.resources.push_back(aliasedBuffer);

        initial_state = GfxAccessDiscard;

        RE_ASSERT(aliasedBuffer.resource != nullptr);
        return (IGfxBuffer*)aliasedBuffer.resource;
    }

    AllocateHeap(buffer_size);
    return AllocateBuffer(firstPass, lastPass, desc, name, initial_state);
}

void RenderGraphResourceAllocator::AllocateHeap(uint32_t size)
{
    GfxHeapDesc heapDesc;
    heapDesc.size = RoundUpPow2(size, 64u * 1024);

    eastl::string heapName = fmt::format("RG Heap {:.1f} MB", heapDesc.size / (1024.0f * 1024.0f)).c_str();

    Heap heap;
    heap.heap = m_pDevice->CreateHeap(heapDesc, heapName);
    m_allocatedHeaps.push_back(heap);
}

void RenderGraphResourceAllocator::Free(IGfxResource* resource, GfxAccessFlags state)
{
    if (resource != nullptr)
    {
        for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
        {
            Heap& heap = m_allocatedHeaps[i];

            for (size_t j = 0; j < heap.resources.size(); ++j)
            {
                AliasedResource& aliasedResource = heap.resources[j];
                if (aliasedResource.resource == resource)
                {
                    aliasedResource.lifetime.Reset();
                    aliasedResource.lastUsedFrame = m_pDevice->GetFrameID();
                    aliasedResource.lastUsedState = state;

                    return;
                }
            }
        }

        RE_ASSERT(false);
    }
}

IGfxResource* RenderGraphResourceAllocator::GetAliasedPrevResource(IGfxResource* resource, uint32_t firstPass)
{
    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        const Heap& heap = m_allocatedHeaps[i];
        if (!heap.Contains(resource))
        {
            continue;
        }

        IGfxResource* prev_resource = nullptr;
        uint32_t prev_resource_lastpass = 0;

        for (size_t j = 0; j < heap.resources.size(); ++j)
        {
            const AliasedResource& aliasedResource = heap.resources[j];

            if (aliasedResource.resource != resource &&
                aliasedResource.lifetime.lastPass < firstPass &&
                aliasedResource.lifetime.lastPass > prev_resource_lastpass)
            {
                prev_resource = aliasedResource.resource;
                prev_resource_lastpass = aliasedResource.lifetime.lastPass;
            }
        }

        return prev_resource;
    }

    RE_ASSERT(false);
    return nullptr;
}

IGfxTexture* RenderGraphResourceAllocator::AllocateNonOverlappingTexture(const GfxTextureDesc& desc, const eastl::string& name, GfxAccessFlags& initial_state)
{
    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); ++iter)
    {
        IGfxTexture* texture = iter->texture;
        if (texture->GetDesc() == desc)
        {
            initial_state = iter->lastUsedState;
            m_freeOverlappingTextures.erase(iter);
            return texture;
        }
    }
    if (IsDepthFormat(desc.format))
    {
        initial_state = GfxAccessDSV;
    }
    else if (desc.usage & GfxTextureUsageRenderTarget)
    {
        initial_state = GfxAccessDSVReadOnly;
    }
    else if (desc.usage & GfxTextureUsageUnorderedAccess)
    {
        initial_state = GfxAccessMaskUAV;
    }

    return m_pDevice->CreateTexture(desc, "RGTexture " + name);
}

void RenderGraphResourceAllocator::FreeNonOverlappingTexture(IGfxTexture* texture, GfxAccessFlags state)
{
    if (texture != nullptr)
    {
        m_freeOverlappingTextures.push_back({ texture, state, m_pDevice->GetFrameID() });
    }
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
