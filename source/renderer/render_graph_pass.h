#pragma once

#include "directed_acyclic_graph.h"
#include <functional>

class RenderGraphResource;

class RenderGraphPassBase : public DAGNode
{
public:
    RenderGraphPassBase(const std::string& name, DirectedAcyclicGraph& graph) : DAGNode(graph)
    {
        m_name = name;
    }

    virtual void Execute() = 0;

    virtual std::string GetGraphvizName() const override { return m_name.c_str(); }
    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "darkgoldenrod1" : "darkgoldenrod4"; }

protected:
    std::string m_name;
};

template<class T>
class RenderGraphPass : public RenderGraphPassBase
{
public:
    RenderGraphPass(const std::string& name, DirectedAcyclicGraph& graph, const std::function<void(const T&)>& execute) :
        RenderGraphPassBase(name, graph)
    {
        m_execute = execute;
    }

    void Execute() override
    {
        m_execute(m_parameters);
    }

    T& GetData() { return m_parameters; }
    T const* operator->() { return &GetData(); }

protected:
    T m_parameters;
    std::function<void(const T&)> m_execute;
};

