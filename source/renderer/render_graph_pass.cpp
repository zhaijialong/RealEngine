#include "render_graph_pass.h"
#include "render_graph.h"

RenderGraphPassBase::RenderGraphPassBase(const std::string& name, DirectedAcyclicGraph& graph) :
    DAGNode(graph)
{
    m_name = name;
}

void RenderGraphPassBase::Resolve(const DirectedAcyclicGraph& graph)
{
    std::vector<DAGEdge*> incoming_edge = graph.GetIncomingEdges(this);

    for (size_t i = 0; i < incoming_edge.size(); ++i)
    {
        RenderGraphEdge* edge = (RenderGraphEdge*)incoming_edge[i];
        RE_ASSERT(edge->GetToNode() == this->GetId());

        RenderGraphResourceNode* resource_node = (RenderGraphResourceNode*)graph.GetNode(edge->GetFromNode());
        RenderGraphResource* resource = resource_node->GetResource();

        std::vector<DAGEdge*> resource_incoming = graph.GetIncomingEdges(resource_node);
        RE_ASSERT(resource_incoming.size() <= 1); //应该只有1个或者0个pass输出到这个RT

        bool first_use = resource_node->GetVersion() == 0;
        if (first_use)
        {
            GfxResourceState initial_state = resource->GetInitialState();
            if (initial_state != edge->GetUsage())
            {
                ResourceBarrier barrier;
                barrier.resource = resource_node->GetResource();
                barrier.sub_resource = edge->GetSubresource();
                barrier.old_state = initial_state;
                barrier.new_state = edge->GetUsage();

                m_resourceBarriers.push_back(barrier);
            }
        }
        else
        {
            RenderGraphEdge* before_edge = (RenderGraphEdge*)resource_incoming[0];

            //todo : subresource需要再仔细考虑
            RE_ASSERT(before_edge->GetSubresource() == edge->GetSubresource());

            if (before_edge->GetUsage() != edge->GetUsage())
            {
                ResourceBarrier barrier;
                barrier.resource = resource_node->GetResource();
                barrier.sub_resource = edge->GetSubresource();
                barrier.old_state = before_edge->GetUsage();
                barrier.new_state = edge->GetUsage();

                m_resourceBarriers.push_back(barrier);
            }
        }
    }
}

void RenderGraphPassBase::Begin(IGfxCommandList* pCommandList)
{
    for (size_t i = 0; i < m_resourceBarriers.size(); ++i)
    {
        const ResourceBarrier& barrier = m_resourceBarriers[i];

        pCommandList->ResourceBarrier(barrier.resource->GetResource(), barrier.sub_resource, barrier.old_state, barrier.new_state);
    }
}

void RenderGraphPassBase::End(IGfxCommandList* pCommandList)
{
    //todo
}
