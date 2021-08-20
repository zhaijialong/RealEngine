#pragma once

#include <vector>
#include <string>

using DAGNodeID = uint32_t;

class DirectedAcyclicGraph;
class DAGNode;

class DAGEdge
{
    friend class DirectedAcyclicGraph;
public:
    DAGEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to);
    virtual ~DAGEdge() {}

    DAGNodeID GetFromNode() const { return m_from; }
    DAGNodeID GetToNode() const { return m_to; }

private:
    const DAGNodeID m_from;
    const DAGNodeID m_to;
};

class DAGNode
{
    friend class DirectedAcyclicGraph;
public:
    DAGNode(DirectedAcyclicGraph& graph);
    virtual ~DAGNode() {}

    DAGNodeID GetId() const { return m_ID; }

    // Prevents this node from being culled. Must be called before culling.
    void MakeTarget() { m_nRefCount = TARGET; }

    bool IsTarget() const { return m_nRefCount >= TARGET; }
    bool IsCulled() const { return m_nRefCount == 0; }
    uint32_t GetRefCount() const { return IsTarget() ? 1 : m_nRefCount; }

    virtual char const* GetName() const { return "unknown"; }
    virtual std::string Graphvizify() const;
    virtual std::string GraphvizifyEdgeColor() const;

private:
    DAGNodeID m_ID;
    uint32_t m_nRefCount = 0;

    static const uint32_t TARGET = 0x80000000u;
};

class DirectedAcyclicGraph
{
public:
    DAGNodeID GenerateNodeId() { return (DAGNodeID)m_nodes.size(); }
    DAGNode* GetNode(DAGNodeID id) const { return m_nodes[id]; }
    DAGEdge* GetEdge(DAGNodeID from, DAGNodeID to) const;

    void RegisterNode(DAGNode* node);
    void RegisterEdge(DAGEdge* edge);

    void Clear();
    void Cull();
    bool IsEdgeValid(const DAGEdge* edge) const;

    std::vector<DAGEdge*> GetIncomingEdges(const DAGNode* node) const;
    std::vector<DAGEdge*> GetOutgoingEdges(const DAGNode* node) const;

    //dot.exe -Tpng -O file
    void ExportGraphviz(const char* file);
private:
    std::vector<DAGNode*> m_nodes;
    std::vector<DAGEdge*> m_edges;
};