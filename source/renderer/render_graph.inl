#pragma once

#include "render_graph.h"
#include "render_graph_builder.h"

class RenderGraphResourceNode : public DAGNode
{
public:
    RenderGraphResourceNode(DirectedAcyclicGraph& graph, RenderGraphResource* resource) :
        DAGNode(graph)
    {
        m_pResource = resource;
    }

    virtual char const* GetName() const override { return m_pResource->GetName(); }

private:
    RenderGraphResource* m_pResource;
};

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
    auto* node = new RenderGraphResourceNode(m_graph, resource);

    RenderGraphHandle handle;
    handle.index = (uint16_t)m_resources.size();
    handle.node = (uint32_t)m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}