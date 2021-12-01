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
    DAGNodeID GetFirstPassID() const { return m_firstPass; }
    DAGNodeID GetLastPassID() const { return m_lastPass; }

    void Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass);
    bool IsUsed() const { return m_firstPass != UINT32_MAX; }
    bool IsImported() const { return m_bImported; }

    GfxResourceState GetFinalState() const { return m_lastState; }
    void SetFinalState(GfxResourceState state) { m_lastState = state; }

    bool IsOutput() const { return m_bOutput; }
    void SetOutput(bool value) { m_bOutput = value; }

    bool IsOverlapping() const { return !IsImported() && !IsOutput(); }

    virtual IGfxResource* GetAliasedPrevResource() = 0;

protected:
    std::string m_name;

    DAGNodeID m_firstPass = UINT32_MAX;
    DAGNodeID m_lastPass = 0;
    GfxResourceState m_lastState = GfxResourceState::Common;

    bool m_bImported = false;
    bool m_bOutput = false;
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

    RenderGraphTexture(RenderGraphResourceAllocator& allocator, IGfxTexture* texture, GfxResourceState state) :
        RenderGraphResource(texture->GetName()),
        m_allocator(allocator)
    {
        m_desc = texture->GetDesc();
        m_pTexture = texture;
        m_initialState = state;
        m_bImported = true;
    }

    ~RenderGraphTexture()
    {
        if (!m_bImported)
        {
            if (m_bOutput)
            {
                m_allocator.FreeNonOverlappingTexture(m_pTexture, m_lastState);
            }
            else
            {
                m_allocator.Free(m_pTexture, m_lastState);
            }
        }
    }

    IGfxTexture* GetTexture() const { return m_pTexture; }
    IGfxDescriptor* GetSRV() { return m_allocator.GetDescriptor(m_pTexture, GfxShaderResourceViewDesc()); }
    IGfxDescriptor* GetUAV() { return m_allocator.GetDescriptor(m_pTexture, GfxUnorderedAccessViewDesc()); }
    IGfxDescriptor* GetUAV(uint32_t mip, uint32_t slice)
    {
        GfxUnorderedAccessViewDesc desc;
        desc.texture.mip_slice = mip;
        desc.texture.array_slice = slice;
        return m_allocator.GetDescriptor(m_pTexture, desc);
    }

    virtual void Realize() override
    {
        if (!m_bImported)
        {
            if (m_bOutput)
            {
                m_pTexture = m_allocator.AllocateNonOverlappingTexture(m_desc, m_name, m_initialState);
            }
            else
            {
                m_pTexture = m_allocator.AllocateTexture(m_firstPass, m_lastPass, m_desc, m_name, m_initialState);
            }
        }
    }

    virtual IGfxResource* GetResource() override { return m_pTexture; }
    virtual GfxResourceState GetInitialState() override { return m_initialState; }

    virtual IGfxResource* GetAliasedPrevResource() override
    {
        return m_allocator.GetAliasedPrevResource(m_pTexture, m_firstPass);
    }

private:
    Desc m_desc;
    IGfxTexture* m_pTexture = nullptr;
    GfxResourceState m_initialState = GfxResourceState::Common;
    RenderGraphResourceAllocator& m_allocator;
};

class RenderGraphBuffer : public RenderGraphResource
{

};