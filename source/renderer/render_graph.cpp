#include "render_graph.h"

class RenderGraphEdge : public DAGEdge
{
public:
    RenderGraphEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, GfxResourceState usage, uint32_t subresource) :
        DAGEdge(graph, from, to)
    {
        m_usage = usage;
        m_subresource = subresource;
    }

private:
    GfxResourceState m_usage;
    uint32_t m_subresource;
};


void RenderGraph::Clear()
{
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

    new RenderGraphEdge(m_graph, input_node, pass, usage, subresource);

    return input;
}

RenderGraphHandle RenderGraph::Write(DAGNode* pass, const RenderGraphHandle& output, GfxResourceState usage, uint32_t subresource)
{
    RenderGraphResourceNode* output_node = m_resourceNodes[output.node];

    new RenderGraphEdge(m_graph, pass, output_node, usage, subresource);

    return output;
}
