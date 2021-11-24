#include "render_graph_resource.h"
#include "render_graph.h"

void RenderGraphResource::Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass)
{
    //todo : for resources used in async compute, we should extend its lifetime range
    if (pass->GetId() <= m_firstPass)
    {
        m_firstPass = pass->GetId();
    }

    if (pass->GetId() >= m_lastPass)
    {
        m_lastPass = pass->GetId();
        m_lastState = edge->GetUsage();
    }
}