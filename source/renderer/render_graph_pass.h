#pragma once

#include <string>
#include <vector>
#include <functional>

class RenderGraphBuilder;
class RenderGraphResource;

class RenderPass
{
public:
    RenderPass(const std::string& name) : m_name(name) {}
    virtual ~RenderPass() {}

    virtual void Setup(RenderGraphBuilder& builder) = 0;
    virtual void Execute() = 0;

protected:
    std::string m_name;
    bool m_bIngoreCulling = false;
    uint32_t m_nRefCount = 0;

    std::vector<const RenderGraphResource*> m_creates;
    std::vector<const RenderGraphResource*> m_reads;
    std::vector<const RenderGraphResource*> m_writes;

    friend class RenderGraphBuilder;
};

template<class T>
class LambdaRenderPass : public RenderPass
{
public:
    LambdaRenderPass(const std::string& name,
        const std::function<void(T&, RenderGraphBuilder&)>& setup,
        const std::function<void(const T&)> execute) :
        RenderPass(name)
    {
        m_setup = setup;
        m_execute = execute;
    }

private:
    virtual void Setup(RenderGraphBuilder& builder) override
    {
        m_setup(m_parameters, builder);
    }

    virtual void Execute() override
    {
        m_execute(m_parameters);
    }

protected:
    T m_parameters;
    std::function<void(T&, RenderGraphBuilder&)> m_setup;
    std::function<void(const T&)> m_execute;
};

