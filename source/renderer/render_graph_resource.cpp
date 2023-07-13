#include "render_graph_resource.h"
#include "render_graph.h"

void RenderGraphResource::Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass)
{
    if (pass->GetId() >= m_lastPass)
    {
        m_lastState = edge->GetUsage();
    }

    m_firstPass = eastl::min(m_firstPass, pass->GetId());
    m_lastPass = eastl::max(m_lastPass, pass->GetId());

    //for resources used in async compute, we should extend its lifetime range
    if (pass->GetType() == RenderPassType::AsyncCompute)
    {
        m_firstPass = eastl::min(m_firstPass, pass->GetWaitGraphicsPassID());
        m_lastPass = eastl::max(m_lastPass, pass->GetSignalGraphicsPassID());
    }
}

RGTexture::RGTexture(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RGTexture::RGTexture(RenderGraphResourceAllocator& allocator, IGfxTexture* texture, GfxAccessFlags state) :
    RenderGraphResource(texture->GetName()),
    m_allocator(allocator)
{
    m_desc = texture->GetDesc();
    m_pTexture = texture;
    m_initialState = state;
    m_bImported = true;
}

RGTexture::~RGTexture()
{
    if (!m_bImported)
    {
        if (m_bOutput)
        {
            m_allocator.FreeNonOverlappingTexture(m_pTexture, m_lastState);
        }
        else
        {
            m_allocator.Free(m_pTexture, m_lastState, m_bOutput);
        }
    }
}

IGfxDescriptor* RGTexture::GetSRV()
{
    RE_ASSERT(!IsImported()); 
    return m_allocator.GetDescriptor(m_pTexture, GfxShaderResourceViewDesc());
}

IGfxDescriptor* RGTexture::GetUAV()
{
    RE_ASSERT(!IsImported()); 
    return m_allocator.GetDescriptor(m_pTexture, GfxUnorderedAccessViewDesc());
}

IGfxDescriptor* RGTexture::GetUAV(uint32_t mip, uint32_t slice)
{
    RE_ASSERT(!IsImported());
    GfxUnorderedAccessViewDesc desc;
    desc.texture.mip_slice = mip;
    desc.texture.array_slice = slice;
    return m_allocator.GetDescriptor(m_pTexture, desc);
}

void RGTexture::Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass)
{
    RenderGraphResource::Resolve(edge, pass);

    GfxAccessFlags usage = edge->GetUsage();
    if (usage & GfxAccessRTV)
    {
        m_desc.usage |= GfxTextureUsageRenderTarget;
    }

    if (usage & GfxAccessMaskUAV)
    {
        m_desc.usage |= GfxTextureUsageUnorderedAccess;
    }

    if (usage & (GfxAccessDSV | GfxAccessDSVReadOnly))
    {
        m_desc.usage |= GfxTextureUsageDepthStencil;
    }
}

void RGTexture::Realize()
{
    if (!m_bImported)
    {
        if (m_bOutput)
        {
            m_pTexture = m_allocator.AllocateNonOverlappingTexture(m_desc, m_name, m_initialState);
        }
        else
        {
            m_pTexture = m_allocator.AllocateTexture(m_firstPass, m_lastPass, m_lastState, m_desc, m_name, m_initialState);
        }
    }
}

void RGTexture::Barrier(IGfxCommandList* pCommandList, uint32_t subresource, GfxAccessFlags acess_before, GfxAccessFlags acess_after)
{
    pCommandList->TextureBarrier(m_pTexture, subresource, acess_before, acess_after);
}

IGfxResource* RGTexture::GetAliasedPrevResource(GfxAccessFlags& lastUsedState)
{
    return m_allocator.GetAliasedPrevResource(m_pTexture, m_firstPass, lastUsedState);
}

RGBuffer::RGBuffer(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RGBuffer::RGBuffer(RenderGraphResourceAllocator& allocator, IGfxBuffer* buffer, GfxAccessFlags state) :
    RenderGraphResource(buffer->GetName()),
    m_allocator(allocator)
{
    m_desc = buffer->GetDesc();
    m_pBuffer = buffer;
    m_initialState = state;
    m_bImported = true;
}

RGBuffer::~RGBuffer()
{
    if (!m_bImported)
    {
        m_allocator.Free(m_pBuffer, m_lastState, m_bOutput);
    }
}

IGfxDescriptor* RGBuffer::GetSRV()
{
    RE_ASSERT(!IsImported());

    const GfxBufferDesc& bufferDesc = m_pBuffer->GetDesc();
    GfxShaderResourceViewDesc desc;

    if (bufferDesc.usage & GfxBufferUsageStructuredBuffer)
    {
        desc.type = GfxShaderResourceViewType::StructuredBuffer;
    }
    else if (bufferDesc.usage & GfxBufferUsageTypedBuffer)
    {
        desc.type = GfxShaderResourceViewType::TypedBuffer;
    }
    else if (bufferDesc.usage & GfxBufferUsageRawBuffer)
    {
        desc.type = GfxShaderResourceViewType::RawBuffer;
    }

    desc.buffer.offset = 0;
    desc.buffer.size = bufferDesc.size;

    return m_allocator.GetDescriptor(m_pBuffer, desc);
}

IGfxDescriptor* RGBuffer::GetUAV()
{
    RE_ASSERT(!IsImported());

    const GfxBufferDesc& bufferDesc = m_pBuffer->GetDesc();
    RE_ASSERT(bufferDesc.usage & GfxBufferUsageUnorderedAccess);

    GfxUnorderedAccessViewDesc desc;

    if (bufferDesc.usage & GfxBufferUsageStructuredBuffer)
    {
        desc.type = GfxUnorderedAccessViewType::StructuredBuffer;
    }
    else if (bufferDesc.usage & GfxBufferUsageTypedBuffer)
    {
        desc.type = GfxUnorderedAccessViewType::TypedBuffer;
    }
    else if (bufferDesc.usage & GfxBufferUsageRawBuffer)
    {
        desc.type = GfxUnorderedAccessViewType::RawBuffer;
    }

    desc.buffer.offset = 0;
    desc.buffer.size = bufferDesc.size;

    return m_allocator.GetDescriptor(m_pBuffer, desc);
}

void RGBuffer::Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass)
{
    RenderGraphResource::Resolve(edge, pass);

    if (edge->GetUsage() & GfxAccessMaskUAV)
    {
        m_desc.usage |= GfxBufferUsageUnorderedAccess;
    }
}

void RGBuffer::Realize()
{
    if (!m_bImported)
    {
        m_pBuffer = m_allocator.AllocateBuffer(m_firstPass, m_lastPass, m_lastState, m_desc, m_name, m_initialState);
    }
}

void RGBuffer::Barrier(IGfxCommandList* pCommandList, uint32_t subresource, GfxAccessFlags acess_before, GfxAccessFlags acess_after)
{
    pCommandList->BufferBarrier(m_pBuffer, acess_before, acess_after);
}

IGfxResource* RGBuffer::GetAliasedPrevResource(GfxAccessFlags& lastUsedState)
{
    return m_allocator.GetAliasedPrevResource(m_pBuffer, m_firstPass, lastUsedState);
}
