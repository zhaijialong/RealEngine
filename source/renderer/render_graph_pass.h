#pragma once

#include "directed_acyclic_graph.h"
#include "gfx/gfx.h"
#include "EASTL/functional.h"

class RenderGraph;
class RenderGraphResource;
class RenderGraphEdgeColorAttchment;
class RenderGraphEdgeDepthAttchment;

enum class RenderPassType
{
    Graphics,
    Compute,
    Copy,
    Resolve
};

class RenderGraphPassBase : public DAGNode
{
public:
    RenderGraphPassBase(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph);

    void Resolve(const DirectedAcyclicGraph& graph);
    void Execute(const RenderGraph& graph, IGfxCommandList* pCommandList);

    virtual eastl::string GetGraphvizName() const override { return m_name.c_str(); }
    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "darkgoldenrod1" : "darkgoldenrod4"; }

    void EnableAsyncCompute() { m_bAsyncCompute = true; }
    void BeginEvent(const eastl::string& name) { m_eventNames.push_back(name); }
    void EndEvent() { m_nEndEventNum++; }

    RenderPassType GetType() const { return m_type; }

private:
    void Begin(const RenderGraph& graph, IGfxCommandList* pCommandList);
    void End(IGfxCommandList* pCommandList);

    bool HasGfxRenderPass() const;

    virtual void ExecuteImpl(IGfxCommandList* pCommandList) = 0;

protected:
    eastl::string m_name;
    RenderPassType m_type;
    bool m_bAsyncCompute = false;

    eastl::vector<eastl::string> m_eventNames;
    uint32_t m_nEndEventNum = 0;

    struct ResourceBarrier
    {
        RenderGraphResource* resource;
        uint32_t sub_resource;
        GfxResourceState old_state;
        GfxResourceState new_state;
    };
    eastl::vector<ResourceBarrier> m_resourceBarriers;

    struct AliasBarrier
    {
        IGfxResource* before;
        IGfxResource* after;
    };
    eastl::vector<AliasBarrier> m_aliasBarriers;

    RenderGraphEdgeColorAttchment* m_pColorRT[8] = {};
    RenderGraphEdgeDepthAttchment* m_pDepthRT = nullptr;
};

template<class T>
class RenderGraphPass : public RenderGraphPassBase
{
public:
    RenderGraphPass(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph, const eastl::function<void(const T&, IGfxCommandList*)>& execute) :
        RenderGraphPassBase(name, type, graph)
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
    eastl::function<void(const T&, IGfxCommandList*)> m_execute;
};

