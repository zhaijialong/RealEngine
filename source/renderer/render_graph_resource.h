#pragma once

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
    virtual void Derealize() = 0;

    const char* GetName() const { return m_name.c_str(); }

protected:
    std::string m_name;
};

class RenderGraphTexture : public RenderGraphResource
{
public:
    using Desc = GfxTextureDesc;

    RenderGraphTexture(const std::string& name, const Desc& desc) : RenderGraphResource(name)
    {
        m_desc = desc;
    }

    virtual void Realize() override {}
    virtual void Derealize() override {}

private:
    Desc m_desc;
};

class RenderGraphBuffer : public RenderGraphResource
{

};