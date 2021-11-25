#include "clustered_shading.h"
#include "../renderer.h"
#include "core/engine.h"

ClusteredShading::ClusteredShading(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("clustered_shading.hlsl", "main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "ClusteredShading PSO");
}

void ClusteredShading::Draw(IGfxCommandList* pCommandList, const ClusterShadingPassData& data, uint32_t width, uint32_t height)
{
    RenderGraph* pRenderGraph = m_pRenderer->GetRenderGraph();

    RenderGraphTexture* albedoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inAlbedoRT);
    RenderGraphTexture* normalRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inNormalRT);
    RenderGraphTexture* emissiveRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inEmissiveRT);
    RenderGraphTexture* depthRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inDepthRT);
    RenderGraphTexture* shadowRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inShadowRT);
    RenderGraphTexture* gtaoRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.inAOTermRT);
    RenderGraphTexture* hdrRT = (RenderGraphTexture*)pRenderGraph->GetResource(data.outHdrRT);

    pCommandList->SetPipelineState(m_pPSO);

    struct CB0
    {
        uint width;
        uint height;
        float invWidth;
        float invHeight;
    };

    CB0 cb0 = { width, height, 1.0f / width, 1.0f / height };
    pCommandList->SetComputeConstants(0, &cb0, sizeof(cb0));

    struct CB1
    {
        uint albedoRT;
        uint normalRT;
        uint emissiveRT;
        uint depthRT;

        uint shadowRT;
        uint gtaoRT;
        uint hdrRT;
        uint _padding;
    };

    CB1 cb1;
    cb1.albedoRT = albedoRT->GetSRV()->GetHeapIndex();
    cb1.normalRT = normalRT->GetSRV()->GetHeapIndex();
    cb1.emissiveRT = emissiveRT->GetSRV()->GetHeapIndex();
    cb1.depthRT = depthRT->GetSRV()->GetHeapIndex();
    cb1.shadowRT = shadowRT->GetSRV()->GetHeapIndex();
    cb1.gtaoRT = gtaoRT->GetSRV()->GetHeapIndex();
    cb1.hdrRT = hdrRT->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb1, sizeof(cb1));

    pCommandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}
