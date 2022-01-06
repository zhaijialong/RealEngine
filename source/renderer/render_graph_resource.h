#pragma once

#include "directed_acyclic_graph.h"
#include "gfx/gfx.h"
#include "utils/assert.h"

class RenderGraphEdge;
class RenderGraphPassBase;
class RenderGraphResourceAllocator;

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

    RenderGraphTexture(RenderGraphResourceAllocator& allocator, const std::string& name, const Desc& desc);
    RenderGraphTexture(RenderGraphResourceAllocator& allocator, IGfxTexture* texture, GfxResourceState state);
    ~RenderGraphTexture();

    IGfxTexture* GetTexture() const { return m_pTexture; }
    IGfxDescriptor* GetSRV();
    IGfxDescriptor* GetUAV();
    IGfxDescriptor* GetUAV(uint32_t mip, uint32_t slice);

    virtual void Realize() override;
    virtual IGfxResource* GetResource() override { return m_pTexture; }
    virtual GfxResourceState GetInitialState() override { return m_initialState; }
    virtual IGfxResource* GetAliasedPrevResource() override;

private:
    Desc m_desc;
    IGfxTexture* m_pTexture = nullptr;
    GfxResourceState m_initialState = GfxResourceState::Common;
    RenderGraphResourceAllocator& m_allocator;
};

class RenderGraphBuffer : public RenderGraphResource
{
public:
    using Desc = GfxBufferDesc;

    RenderGraphBuffer(RenderGraphResourceAllocator& allocator, const std::string& name, const Desc& desc);
    RenderGraphBuffer(RenderGraphResourceAllocator& allocator, IGfxBuffer* buffer, GfxResourceState state);
    ~RenderGraphBuffer();

    IGfxBuffer* GetBuffer() const { return m_pBuffer; }
    IGfxDescriptor* GetSRV();
    IGfxDescriptor* GetUAV();

    virtual void Realize() override;
    virtual IGfxResource* GetResource() override { return m_pBuffer; }
    virtual GfxResourceState GetInitialState() override { return m_initialState; }
    virtual IGfxResource* GetAliasedPrevResource() override;

private:
    Desc m_desc;
    IGfxBuffer* m_pBuffer = nullptr;
    GfxResourceState m_initialState = GfxResourceState::Common;
    RenderGraphResourceAllocator& m_allocator;
};