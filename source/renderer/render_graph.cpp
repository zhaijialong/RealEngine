#include "render_graph.h"
#include "core/engine.h"

void RenderGraph::Clear()
{
    for (size_t i = 0; i < m_objFinalizer.size(); ++i)
    {
        m_objFinalizer[i].finalizer(m_objFinalizer[i].obj);
    }
    m_objFinalizer.clear();

    m_graph.Clear();

    m_passes.clear();
    m_resourceNodes.clear();
    m_resources.clear();

    m_allocator.Reset();
    m_resourceAllocator.Reset();
}

void RenderGraph::Compile()
{
    m_graph.Cull();

    //allocate resources (todo : aliasing)
    for (size_t i = 0; i < m_resourceNodes.size(); ++i)
    {
        RenderGraphResourceNode* node = m_resourceNodes[i];
        if (node->IsCulled())
        {
            continue;
        }

        if (node->GetVersion() == 0)
        {
            RenderGraphResource* resource = node->GetResource();
            resource->Realize(m_resourceAllocator);
        }
    }

    //resolve barriers
    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pass = m_passes[i];
        if (pass->IsCulled())
        {
            continue;
        }

        //todo
    }
}

void RenderGraph::Execute(IGfxCommandList* pCommandList)
{
    RENDER_EVENT(pCommandList, "RenderGraph");

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pass = m_passes[i];
        if (pass->IsCulled())
        {
            continue;
        }

        pass->FlushBarriers();

        RENDER_EVENT(pCommandList, pass->GetName());
        pass->Execute(pCommandList);
    }
}

void RenderGraph::Present(const RenderGraphHandle& handle)
{
    RenderGraphResourceNode* node = m_resourceNodes[handle.node];
    node->MakeTarget();
}

RenderGraphResource* RenderGraph::GetResource(const RenderGraphHandle& handle)
{
    return m_resources[handle.index];
}

bool RenderGraph::Export(const std::string& file)
{
    return m_graph.ExportGraphviz(file.c_str());
}

RenderGraphHandle RenderGraph::Read(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];

    AllocatePOD<RenderGraphEdge>(m_graph, input_node, pass, usage, subresource);

    return input;
}

RenderGraphHandle RenderGraph::Write(RenderGraphPassBase* pass, const RenderGraphHandle& input, GfxResourceState usage, uint32_t subresource)
{
    RenderGraphResource* resource = m_resources[input.index];

    RenderGraphResourceNode* input_node = m_resourceNodes[input.node];
    AllocatePOD<RenderGraphEdge>(m_graph, input_node, pass, usage, subresource);

    RenderGraphResourceNode* output_node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, input_node->GetVersion() + 1);
    AllocatePOD<RenderGraphEdge>(m_graph, pass, output_node, usage, subresource);

    RenderGraphHandle output;        
    output.index = input.index;
    output.node = (uint16_t)m_resourceNodes.size();

    m_resourceNodes.push_back(output_node);

    return output;
}
