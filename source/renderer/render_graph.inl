#pragma once

#include "render_graph.h"
#include "render_graph_builder.h"

template<typename Data, typename Setup, typename Exec>
inline RenderGraphPass<Data>& RenderGraph::AddPass(const char* name, const Setup& setup, const Exec& execute)
{
    auto* pass = new RenderGraphPass<Data>(name, m_graph, execute);

    RenderGraphBuilder builder(this, pass);
    setup(pass->GetData(), builder);

    return *pass;
}

template<typename Resource>
inline RenderGraphHandle RenderGraph::Create(const char* name, const typename Resource::Desc& desc)
{
    auto* resource = new Resource(name, desc);

    RenderGraphHandle handle;
    handle.index = (uint16_t)m_resources.size();

    m_resources.push_back(resource);

    return handle;
}