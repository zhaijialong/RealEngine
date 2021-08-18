#include "directed_acyclic_graph.h"
#include "utils/assert.h"

DAGEdge::DAGEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to) :
    from(from->GetId()),
    to(to->GetId())
{
    RE_ASSERT(graph.GetNode(this->from) == from);
    RE_ASSERT(graph.GetNode(this->to) == to);

    graph.RegisterEdge(this);
}

DAGNode::DAGNode(DirectedAcyclicGraph& graph)
{
    m_ID = graph.GenerateNodeId();

    graph.RegisterNode(this);
}

void DirectedAcyclicGraph::RegisterNode(DAGNode* node)
{
    RE_ASSERT(node->GetId() == m_nodes.size());

    m_nodes.push_back(node);
}

void DirectedAcyclicGraph::RegisterEdge(DAGEdge* edge)
{
    m_edges.push_back(edge);
}

void DirectedAcyclicGraph::Clear()
{
    m_edges.clear();
    m_nodes.clear();
}

void DirectedAcyclicGraph::Cull()
{
    // update reference counts
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        DAGEdge* edge = m_edges[i];
        DAGNode* node = m_nodes[edge->from];
        node->m_nRefCount++;
    }

    // cull nodes with a 0 reference count
    std::vector<DAGNode*> stack;
    for (size_t i = 0; i < m_nodes.size(); ++i) 
    {
        if (m_nodes[i]->GetRefCount() == 0) 
        {
            stack.push_back(m_nodes[i]);
        }
    }

    while (!stack.empty()) 
    {
        DAGNode* node = stack.back();
        stack.pop_back();

        std::vector<DAGEdge*> incoming = GetIncomingEdges(node);

        for (size_t i = 0; i < incoming.size(); ++i) 
        {
            DAGNode* linked_node = GetNode(incoming[i]->from);

            if (--linked_node->m_nRefCount == 0)
            {
                stack.push_back(linked_node);
            }
        }
    }
}

bool DirectedAcyclicGraph::IsEdgeValid(const DAGEdge* edge) const
{
    return !GetNode(edge->from)->IsCulled() && GetNode(edge->to)->IsCulled();
}

std::vector<DAGEdge*> DirectedAcyclicGraph::GetIncomingEdges(const DAGNode* node) const
{
    std::vector<DAGEdge*> result;
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->to == node->GetId())
        {
            result.push_back(m_edges[i]);
        }
    }
    return result;
}

std::vector<DAGEdge*> DirectedAcyclicGraph::GetOutgoingEdges(const DAGNode* node) const
{
    std::vector<DAGEdge*> result;
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->from == node->GetId())
        {
            result.push_back(m_edges[i]);
        }
    }
    return result;
}
