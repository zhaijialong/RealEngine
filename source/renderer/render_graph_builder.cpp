#include "render_graph_builder.h"
#include "render_pass.h"

RenderGraphResource* RenderGraphBuilder::Read(RenderGraphResource* resource)
{
    resource->m_readers.push_back(m_pPass);
    m_pPass->m_reads.push_back(resource);

    return resource;
}

RenderGraphResource* RenderGraphBuilder::Write(RenderGraphResource* resource)
{
    resource->m_writers.push_back(m_pPass);
    m_pPass->m_writes.push_back(resource);

    return resource;
}

RenderGraphTexture* RenderGraphBuilder::CreateTexture(const std::string& name, uint32_t width, uint32_t height)
{
    return nullptr;
}
