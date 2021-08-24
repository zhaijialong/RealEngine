#pragma once

#include <string>
#include <vector>

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

protected:
    std::string m_name;

    friend class RenderGraphBuilder;
};

class RenderGraphTexture : public RenderGraphResource
{
public:
    virtual void Realize() override;
    virtual void Derealize() override;
};

class RenderGraphBuffer : public RenderGraphResource
{

};