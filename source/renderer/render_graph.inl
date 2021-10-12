#pragma once

#include "render_graph.h"
#include "render_graph_builder.h"

class RenderGraphResourceNode : public DAGNode
{
public:
    RenderGraphResourceNode(DirectedAcyclicGraph& graph, RenderGraphResource* resource, uint32_t version) :
        DAGNode(graph)
    {
        m_pResource = resource;
        m_version = version;
    }

    RenderGraphResource* GetResource() const { return m_pResource; }
    uint32_t GetVersion() const { return m_version; }

    virtual std::string GetGraphvizName() const override 
    {
        std::string s = m_pResource->GetName();
        s.append("\nversion:");
        s.append(std::to_string(m_version));
        return s;
    }

    virtual const char* GetGraphvizColor() const { return !IsCulled() ? "lightskyblue1" : "lightskyblue4"; }
    virtual const char* GetGraphvizShape() const { return "ellipse"; }

private:
    RenderGraphResource* m_pResource;
    uint32_t m_version;
};

class RenderGraphEdge : public DAGEdge
{
public:
    RenderGraphEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, GfxResourceState usage, uint32_t subresource) :
        DAGEdge(graph, from, to)
    {
        m_usage = usage;
        m_subresource = subresource;
    }

    GfxResourceState GetUsage() const { return m_usage; }
    uint32_t GetSubresource() const { return m_subresource; }

private:
    GfxResourceState m_usage;
    uint32_t m_subresource;
};

class RenderGraphEdgeColorAttchment : public RenderGraphEdge
{
public:
    RenderGraphEdgeColorAttchment(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, GfxResourceState usage, uint32_t subresource, 
        uint32_t color_index, GfxRenderPassLoadOp load_op, const float4& clear_color) :
        RenderGraphEdge(graph, from, to, usage, subresource)
    {
        m_colorIndex = color_index;
        m_loadOp = load_op;

        m_clearColor[0] = clear_color[0];
        m_clearColor[1] = clear_color[1];
        m_clearColor[2] = clear_color[2];
        m_clearColor[3] = clear_color[3];
    }
    uint32_t GetColorIndex() const { return m_colorIndex; }
    GfxRenderPassLoadOp GetLoadOp() const { return m_loadOp; }
    const float* GetClearColor() const { return m_clearColor; }

private:
    uint32_t m_colorIndex;
    GfxRenderPassLoadOp m_loadOp;
    float m_clearColor[4] = {};
};

class RenderGraphEdgeDepthAttchment : public RenderGraphEdge
{
public:
    RenderGraphEdgeDepthAttchment(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, GfxResourceState usage, uint32_t subresource,
        GfxRenderPassLoadOp depth_load_op, GfxRenderPassLoadOp stencil_load_op, float clear_depth, uint32_t clear_stencil) :
        RenderGraphEdge(graph, from, to, usage, subresource)
    {
        m_depthLoadOp = depth_load_op;
        m_stencilLoadOp = stencil_load_op;
        m_clearDepth = clear_depth;
        m_clearStencil = clear_stencil;
    }

    GfxRenderPassLoadOp GetDepthLoadOp() const { return m_depthLoadOp; };
    GfxRenderPassLoadOp GetStencilLoadOp() const { return m_stencilLoadOp; };
    float GetClearDepth() const { return m_clearDepth; }
    uint32_t GetClearStencil() const { return m_clearStencil; };

private:
    GfxRenderPassLoadOp m_depthLoadOp;
    GfxRenderPassLoadOp m_stencilLoadOp;
    float m_clearDepth;
    uint32_t m_clearStencil;
};

template<typename T>
void ClassFinalizer(void* p)
{
    ((T*)p)->~T();
}

template<typename T, typename... ArgsT>
inline T* RenderGraph::Allocate(ArgsT&&... arguments)
{
    T* p = (T*)m_allocator.Alloc(sizeof(T));
    new (p) T(arguments...);

    ObjFinalizer finalizer;
    finalizer.obj = p;
    finalizer.finalizer = &ClassFinalizer<T>;
    m_objFinalizer.push_back(finalizer);

    return p;
}

template<typename T, typename... ArgsT>
inline T* RenderGraph::AllocatePOD(ArgsT&&... arguments)
{
    T* p = (T*)m_allocator.Alloc(sizeof(T));
    new (p) T(arguments...);

    return p;
}

template<typename Data, typename Setup, typename Exec>
inline RenderGraphPass<Data>& RenderGraph::AddPass(const char* name, const Setup& setup, const Exec& execute)
{
    auto* pass = Allocate<RenderGraphPass<Data>>(name, m_graph, execute);

    RenderGraphBuilder builder(this, pass);
    setup(pass->GetData(), builder);

    m_passes.push_back(pass);

    return *pass;
}

template<typename Resource>
inline RenderGraphHandle RenderGraph::Create(const typename Resource::Desc& desc, const char* name)
{
    auto* resource = Allocate<Resource>(m_resourceAllocator, name, desc);
    auto* node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RenderGraphHandle handle;
    handle.index = (uint16_t)m_resources.size();
    handle.node = (uint16_t)m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}
