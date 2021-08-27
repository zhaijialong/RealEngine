#include "render_graph.h"
#include "core/engine.h"

void RenderGraph::Clear()
{
    m_graph.Clear();

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        m_passes[i]->~RenderGraphPassBase();
    }
    m_passes.clear();

    for (size_t i = 0; i < m_resourceNodes.size(); ++i)
    {
        m_resourceNodes[i]->~RenderGraphResourceNode();
    }
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

void RenderGraph::Execute(IGfxCommandList* pCommandList)
{
    
}

void RenderGraph::Present(const RenderGraphHandle& handle)
{
    RenderGraphResourceNode* node = m_resourceNodes[handle.node];
    node->MakeTarget();
}

bool RenderGraph::Export(const std::string& file)
{
    return m_graph.ExportGraphviz(file.c_str());
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
    new (Allocate<RenderGraphEdge>()) RenderGraphEdge(m_graph, input_node, pass, usage, subresource);

    RenderGraphResourceNode* output_node = new (Allocate<RenderGraphResourceNode>()) RenderGraphResourceNode(m_graph, resource, input_node->GetVersion() + 1);
    new (Allocate<RenderGraphEdge>()) RenderGraphEdge(m_graph, pass, output_node, usage, subresource);

    RenderGraphHandle output;        
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}
