#include "render_graph.h"

void RenderGraph::Clear()
{
    m_graph.Clear();
    m_resourceNodes.clear();

    for (size_t i = 0; i < m_resources.size(); ++i)
    {
        m_resources[i]->~RenderGraphResource();
    }
    m_resources.clear();

    m_allocator.Reset();
}

void RenderGraph::Compile()
{
    m_graph.Cull();
}

void RenderGraph::Execute()
{
    m_graph.ExportGraphviz("A:/rendergraph.dot");
}

void RenderGraph::Present(const RenderGraphHandle& handle)
{
    RenderGraphResourceNode* node = m_resourceNodes[handle.node];
    node->MakeTarget();
}

RenderGraphHandle RenderGraph::Read(DAGNode* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];

    new (Allocate<RenderGraphEdge>()) RenderGraphEdge(m_graph, input_node, pass, usage, subresource);

    return input;
}

RenderGraphHandle RenderGraph::Write(DAGNode* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RenderGraphResource* resource = m_resources[input.index];
    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];

    RenderGraphHandle output;
    RenderGraphResourceNode* output_node;

    if (input_node->GetVersion() == 0)
    {
        output_node = input_node;
        output = input;

        input_node->SetVersion(1);
    }
    else
    {
        uint32_t version = input_node->GetVersion() + 1;
        output_node = new (Allocate<RenderGraphResourceNode>()) RenderGraphResourceNode(m_graph, resource, version);
        
        output.index = input.index;
        output.node = (uint16_t)m_resourceNodes.size();

        m_resourceNodes.push_back(output_node);
    }

    new (Allocate<RenderGraphEdge>()) RenderGraphEdge(m_graph, pass, output_node, usage, subresource);

    return output;
}
