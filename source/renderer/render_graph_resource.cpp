#include "render_graph_resource.h"
#include "render_graph.h"

void RenderGraphResource::Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass)
{
    //todo : for resources used in async compute, we should extend its lifetime range
    if (pass->GetId() <= m_firstPass)
    {
        m_firstPass = pass->GetId();
    }

    if (pass->GetId() >= m_lastPass)
    {
        m_lastPass = pass->GetId();
        m_lastState = edge->GetUsage();
    }
}

RenderGraphTexture::RenderGraphTexture(RenderGraphResourceAllocator& allocator, const std::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RenderGraphTexture::RenderGraphTexture(RenderGraphResourceAllocator& allocator, IGfxTexture* texture, GfxResourceState state) :
    RenderGraphResource(texture->GetName()),
    m_allocator(allocator)
{
    m_desc = texture->GetDesc();
    m_pTexture = texture;
    m_initialState = state;
    m_bImported = true;
}

RenderGraphTexture::~RenderGraphTexture()
{
    if (!m_bImported)
    {
        if (m_bOutput)
        {
            m_allocator.FreeNonOverlappingTexture(m_pTexture, m_lastState);
        }
        else
        {
            m_allocator.Free(m_pTexture, m_lastState);
        }
    }
}

IGfxDescriptor* RenderGraphTexture::GetSRV()
{
    RE_ASSERT(!IsImported()); 
    return m_allocator.GetDescriptor(m_pTexture, GfxShaderResourceViewDesc());
}

IGfxDescriptor* RenderGraphTexture::GetUAV()
{
    RE_ASSERT(!IsImported()); 
    return m_allocator.GetDescriptor(m_pTexture, GfxUnorderedAccessViewDesc());
}

IGfxDescriptor* RenderGraphTexture::GetUAV(uint32_t mip, uint32_t slice)
{
    RE_ASSERT(!IsImported());
    GfxUnorderedAccessViewDesc desc;
    desc.texture.mip_slice = mip;
    desc.texture.array_slice = slice;
    return m_allocator.GetDescriptor(m_pTexture, desc);
}

void RenderGraphTexture::Realize()
{
    if (!m_bImported)
    {
        if (m_bOutput)
        {
            m_pTexture = m_allocator.AllocateNonOverlappingTexture(m_desc, m_name, m_initialState);
        }
        else
        {
            m_pTexture = m_allocator.AllocateTexture(m_firstPass, m_lastPass, m_desc, m_name, m_initialState);
        }
    }
}

IGfxResource* RenderGraphTexture::GetAliasedPrevResource()
{
    return m_allocator.GetAliasedPrevResource(m_pTexture, m_firstPass);
}

RenderGraphBuffer::RenderGraphBuffer(RenderGraphResourceAllocator& allocator, const std::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RenderGraphBuffer::RenderGraphBuffer(RenderGraphResourceAllocator& allocator, IGfxBuffer* buffer, GfxResourceState state) :
    RenderGraphResource(buffer->GetName()),
    m_allocator(allocator)
{
    m_desc = buffer->GetDesc();
    m_pBuffer = buffer;
    m_initialState = state;
    m_bImported = true;
}

RenderGraphBuffer::~RenderGraphBuffer()
{
    if (!m_bImported)
    {
        m_allocator.Free(m_pBuffer, m_lastState);
    }
}

IGfxDescriptor* RenderGraphBuffer::GetSRV()
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

IGfxDescriptor* RenderGraphBuffer::GetUAV()
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

void RenderGraphBuffer::Realize()
{
    if (!m_bImported)
    {
        m_pBuffer = m_allocator.AllocateBuffer(m_firstPass, m_lastPass, m_desc, m_name, m_initialState);
    }
}

IGfxResource* RenderGraphBuffer::GetAliasedPrevResource()
{
    return m_allocator.GetAliasedPrevResource(m_pBuffer, m_firstPass);
}
