#pragma once

#include "directed_acyclic_graph.h"
#include "render_graph_resource_allocator.h"
#include "gfx/gfx.h"

class RenderGraphEdge;
class RenderGraphPassBase;

class RenderGraphResource
{
public:
    RenderGraphResource(const std::string& name)
    {
        m_name = name;
    }
    virtual ~RenderGraphResource() {}

    virtual void Realize() = 0;
    virtual IGfxResource* GetResource() = 0;
    virtual GfxResourceState GetInitialState() = 0;

    const char* GetName() const { return m_name.c_str(); }

    void Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass);

    GfxResourceState GetFinalState() const { return m_lastState; }
    void SetFinalState(GfxResourceState state) { m_lastState = state; }

protected:
    std::string m_name;

    DAGNodeID m_firstPass = 0;
    DAGNodeID m_lastPass = 0;
    GfxResourceState m_lastState = GfxResourceState::Common;
};

class RenderGraphTexture : public RenderGraphResource
{
public:
    using Desc = GfxTextureDesc;

    RenderGraphTexture(RenderGraphResourceAllocator& allocator, const std::string& name, const Desc& desc) : 
        RenderGraphResource(name),
        m_allocator(allocator)
    {
        m_desc = desc;
    }

    ~RenderGraphTexture()
    {
        m_allocator.Free(m_pTexture, m_lastState);
    }

    IGfxTexture* GetTexture() const { return m_pTexture; }
    IGfxDescriptor* GetSRV() { return m_allocator.GetDescriptor(m_pTexture, GfxShaderResourceViewDesc()); }

    virtual void Realize() override
    {
        m_pTexture = m_allocator.AllocateTexture(m_desc, m_name, m_initialState);
    }

    virtual IGfxResource* GetResource() override { return m_pTexture; }
    virtual GfxResourceState GetInitialState() override { return m_initialState; }

private:
    Desc m_desc;
    IGfxTexture* m_pTexture = nullptr;
    GfxResourceState m_initialState = GfxResourceState::Common;
    RenderGraphResourceAllocator& m_allocator;
};

class RenderGraphBuffer : public RenderGraphResource
{

};