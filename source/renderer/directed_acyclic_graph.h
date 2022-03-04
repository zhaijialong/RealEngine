#pragma once

#include "EASTL/vector.h"
#include "EASTL/string.h"

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

    virtual eastl::string GetGraphvizName() const { return "unknown"; }
    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "skyblue" : "skyblue4"; }
    virtual const char* GetGraphvizEdgeColor() const { return "darkolivegreen"; }
    virtual const char* GetGraphvizShape() const { return "rectangle"; }
    eastl::string Graphvizify() const;

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

    eastl::vector<DAGEdge*> GetIncomingEdges(const DAGNode* node) const;
    eastl::vector<DAGEdge*> GetOutgoingEdges(const DAGNode* node) const;

    //dot.exe -Tpng -O file
    bool ExportGraphviz(const char* file);
private:
    eastl::vector<DAGNode*> m_nodes;
    eastl::vector<DAGEdge*> m_edges;
};