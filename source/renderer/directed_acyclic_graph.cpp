#include "directed_acyclic_graph.h"
#include "utils/assert.h"
#include <fstream>
#include <algorithm>

DAGEdge::DAGEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to) :
    m_from(from->GetId()),
    m_to(to->GetId())
{
    RE_ASSERT(graph.GetNode(m_from) == from);
    RE_ASSERT(graph.GetNode(m_to) == to);

    graph.RegisterEdge(this);
}

DAGNode::DAGNode(DirectedAcyclicGraph& graph)
{
    m_ID = graph.GenerateNodeId();

    graph.RegisterNode(this);
}

DAGEdge* DirectedAcyclicGraph::GetEdge(DAGNodeID from, DAGNodeID to) const
{
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->m_from == from && m_edges[i]->m_to == to)
        {
            return m_edges[i];
        }
    }

    return nullptr;
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
        DAGNode* node = m_nodes[edge->m_from];
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
            DAGNode* linked_node = GetNode(incoming[i]->m_from);

            if (--linked_node->m_nRefCount == 0)
            {
                stack.push_back(linked_node);
            }
        }
    }
}

bool DirectedAcyclicGraph::IsEdgeValid(const DAGEdge* edge) const
{
    return !GetNode(edge->m_from)->IsCulled() && !GetNode(edge->m_to)->IsCulled();
}

std::vector<DAGEdge*> DirectedAcyclicGraph::GetIncomingEdges(const DAGNode* node) const
{
    std::vector<DAGEdge*> result;
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->m_to == node->GetId())
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
        if (m_edges[i]->m_from == node->GetId())
        {
            result.push_back(m_edges[i]);
        }
    }
    return result;
}

void DirectedAcyclicGraph::ExportGraphviz(const char* file)
{
    std::ofstream out;
    out.open(file);
    if (out.fail())
    {
        return;
    }

    out << "digraph {\n";
    out << "rankdir = LR\n";
    //out << "bgcolor = black\n";
    out << "node [fontname=\"helvetica\", fontsize=10]\n\n";

    for (size_t i = 0; i < m_nodes.size(); ++i) 
    {
        uint32_t id = m_nodes[i]->GetId();
        std::string s = m_nodes[i]->Graphvizify();
        out << "\"N" << id << "\" " << s << "\n";
    }

    out << "\n";
    for (size_t i = 0; i < m_nodes.size(); ++i) 
    {
        DAGNode* node = m_nodes[i];
        uint32_t id = node->GetId();

        auto edges = GetOutgoingEdges(node);
        auto first = edges.begin();
        auto pos = std::partition(first, edges.end(),
            [this](auto const& edge) { return IsEdgeValid(edge); });

        std::string s = node->GetGraphvizEdgeColor();

        // render the valid edges
        if (first != pos) 
        {
            out << "N" << id << " -> { ";
            while (first != pos) 
            {
                DAGNode const* ref = GetNode((*first++)->m_to);
                out << "N" << ref->GetId() << " ";
            }
            out << "} [color=" << s.c_str() << "2]\n";
        }

        // render the invalid edges
        if (first != edges.end()) 
        {
            out << "N" << id << " -> { ";
            while (first != edges.end()) 
            {
                DAGNode const* ref = GetNode((*first++)->m_to);
                out << "N" << ref->GetId() << " ";
            }
            out << "} [color=" << s.c_str() << "4 style=dashed]\n";
        }
    }

    out << "}" << std::endl;

    out.close();
}

std::string DAGNode::Graphvizify() const
{
    std::string s;
    s.reserve(128);

    s.append("[label=\"");
    s.append(GetGraphvizName());
    s.append("\\nrefs: ");
    s.append(std::to_string(GetRefCount()));
    //s.append(", id: ");
    //s.append(std::to_string(GetId()));
    s.append("\", style=filled, shape=");
    s.append(GetGraphvizShape());
    s.append(", fillcolor = ");
    s.append(!IsCulled() ? "skyblue" : "skyblue4");
    s.append("]");
    s.shrink_to_fit();

    return s;
}

