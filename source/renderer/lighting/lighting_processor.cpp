#include "lighting_processor.h"

LightingProcessor::LightingProcessor(Renderer* pRenderer)
{
    m_pGTAO = std::make_unique<GTAO>(pRenderer);
    m_pClusteredShading = std::make_unique<ClusteredShading>(pRenderer);
}

RenderGraphHandle LightingProcessor::Process(RenderGraph* pRenderGraph, const LightingProcessInput& input, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Lighting");

    RenderGraphHandle gtao = m_pGTAO->Render(pRenderGraph, input.depthRT, input.normalRT, width, height);

    return m_pClusteredShading->Render(pRenderGraph, input.diffuseRT, input.specularRT, input.normalRT, input.emissiveRT, input.depthRT, input.shadowRT, gtao, width, height);
}
