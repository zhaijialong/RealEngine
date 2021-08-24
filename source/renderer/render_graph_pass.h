#pragma once

#include "directed_acyclic_graph.h"
#include <functional>

class RenderGraphResource;

template<class T>
class RenderGraphPass : public DAGNode
{
public:
    RenderGraphPass(const std::string& name, DirectedAcyclicGraph& graph,
        const std::function<void(const T&)>& execute) :
        DAGNode(graph)
    {
        m_name = name;
        m_execute = execute;
    }

    T& GetData() { return m_parameters; }

    void Execute()
    {
        m_execute(m_parameters);
    }

protected:
    std::string m_name;
    T m_parameters;
    std::function<void(const T&)> m_execute;
};

