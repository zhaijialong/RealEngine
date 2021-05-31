#pragma once

#include <string>
#include <vector>

class RenderPass;

class RenderGraphResource
{
public:
    RenderGraphResource(const std::string& name, const RenderPass* creator)
    {
        m_name = name;
        m_pCreator = creator;
    }
    virtual ~RenderGraphResource() {}

    virtual void Realize() = 0;
    virtual void Derealize() = 0;

    //transient or imported
    bool IsTransient() const { return m_pCreator != nullptr; }

protected:
    std::string m_name;
    uint32_t m_nRefCount = 0;
    const RenderPass* m_pCreator = nullptr;
    std::vector<const RenderPass*> m_readers;
    std::vector<const RenderPass*> m_writers;

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