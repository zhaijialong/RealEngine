#pragma once

#include "directed_acyclic_graph.h"
#include "gfx/gfx.h"
#include <functional>

class RenderGraph;
class RenderGraphResource;
class RenderGraphEdgeColorAttchment;
class RenderGraphEdgeDepthAttchment;

class RenderGraphPassBase : public DAGNode
{
public:
    RenderGraphPassBase(const std::string& name, DirectedAcyclicGraph& graph);

    void Resolve(const DirectedAcyclicGraph& graph);
    void Execute(const RenderGraph& graph, IGfxCommandList* pCommandList);

    virtual std::string GetGraphvizName() const override { return m_name.c_str(); }
    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "darkgoldenrod1" : "darkgoldenrod4"; }

private:
    void Begin(const RenderGraph& graph, IGfxCommandList* pCommandList);
    void End(IGfxCommandList* pCommandList);

    bool HasGfxRenderPass() const;

    virtual void ExecuteImpl(IGfxCommandList* pCommandList) = 0;

protected:
    std::string m_name;

    struct ResourceBarrier
    {
        RenderGraphResource* resource;
        uint32_t sub_resource;
        GfxResourceState old_state;
        GfxResourceState new_state;
    };
    std::vector<ResourceBarrier> m_resourceBarriers;

    RenderGraphEdgeColorAttchment* m_pColorRT[8] = {};
    RenderGraphEdgeDepthAttchment* m_pDepthRT = nullptr;
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

    T& GetData() { return m_parameters; }
    T const* operator->() { return &GetData(); }

private:
    void ExecuteImpl(IGfxCommandList* pCommandList) override
    {
        m_execute(m_parameters, pCommandList);
    }    

protected:
    T m_parameters;
    std::function<void(const T&, IGfxCommandList*)> m_execute;
};

