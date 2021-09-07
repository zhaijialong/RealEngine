#pragma once

#include "render_graph_resource_allocator.h"
#include "gfx/gfx.h"

class RenderGraphResource
{
public:
    RenderGraphResource(const std::string& name)
    {
        m_name = name;
    }
    virtual ~RenderGraphResource() {}

    virtual void Realize() = 0;

    const char* GetName() const { return m_name.c_str(); }

protected:
    std::string m_name;

    uint32_t m_firstPass = 0;
    uint32_t m_lastPass = 0;
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
        m_allocator.Free(m_pTexture);
    }

    IGfxTexture* GetTexture() const { return m_pTexture; }
    IGfxDescriptor* GetSRV() { return m_allocator.GetDescriptor(m_pTexture, GfxShaderResourceViewDesc()); }

    virtual void Realize() override
    {
        m_pTexture = m_allocator.AllocateTexture(m_desc, m_name);
    }

private:
    Desc m_desc;
    IGfxTexture* m_pTexture = nullptr;
    RenderGraphResourceAllocator& m_allocator;
};

class RenderGraphBuffer : public RenderGraphResource
{

};