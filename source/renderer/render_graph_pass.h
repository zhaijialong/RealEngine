#pragma once

#include "directed_acyclic_graph.h"
#include "gfx/gfx.h"
#include <functional>

class RenderGraphResource;

class RenderGraphPassBase : public DAGNode
{
public:
    RenderGraphPassBase(const std::string& name, DirectedAcyclicGraph& graph);

    const char* GetName() const { return m_name.c_str(); }
    GPU_TOKEN GetGpuToken() const { return m_token; }

    void Resolve(const DirectedAcyclicGraph& graph);
    void Begin(IGfxCommandList* pCommandList);
    void End(IGfxCommandList* pCommandList);

    virtual void Execute(IGfxCommandList* pCommandList) = 0;

    virtual std::string GetGraphvizName() const override { return m_name.c_str(); }
    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "darkgoldenrod1" : "darkgoldenrod4"; }

protected:
    std::string m_name;
    GPU_TOKEN m_token;

    struct ResourceBarrier
    {
        RenderGraphResource* resource;
        uint32_t sub_resource;
        GfxResourceState old_state;
        GfxResourceState new_state;
    };
    std::vector<ResourceBarrier> m_resourceBarriers;
};

template<class T>
class RenderGraphPass : public RenderGraphPassBase
{
public:
    RenderGraphPass(const std::string& name, DirectedAcyclicGraph& graph, const std::function<void(const T&, IGfxCommandList*)>& execute) :
        RenderGraphPassBase(name, graph)
    {
        m_execute = execute;
    }

    void Execute(IGfxCommandList* pCommandList) override
    {
        m_execute(m_parameters, pCommandList);
    }

    T& GetData() { return m_parameters; }
    T const* operator->() { return &GetData(); }

protected:
    T m_parameters;
    std::function<void(const T&, IGfxCommandList*)> m_execute;
};

