#include "reflection_denoiser.h"
#include "../renderer.h"

ReflectionDenoiser::ReflectionDenoiser(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_reproject.hlsl", "main", "cs_6_6", {});
    m_pReprojectPSO = pRenderer->GetPipelineState(desc, "HSR denoiser reproject PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_prefilter.hlsl", "main", "cs_6_6", {});
    m_pPrefilterPSO = pRenderer->GetPipelineState(desc, "HSR denoiser prefilter PSO");

    desc.cs = pRenderer->GetShader("hybrid_stochastic_reflection/denoiser_resolve_temporal.hlsl", "main", "cs_6_6", {});
    m_pResolveTemporalPSO = pRenderer->GetPipelineState(desc, "HSR denoiser resolve temporal PSO");
}

RenderGraphHandle ReflectionDenoiser::Render(RenderGraph* pRenderGraph, RenderGraphHandle indirectArgs, RenderGraphHandle tileListBuffer, RenderGraphHandle input, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "ReflectionDenoiser");

    struct ReprojectPassData
    {
        RenderGraphHandle indirectArgs;
        RenderGraphHandle tileListBuffer;
        RenderGraphHandle input;
        RenderGraphHandle output;
    };

    auto reproject_pass = pRenderGraph->AddPass<ReprojectPassData>("ReflectionDenoiser - Reproject", RenderPassType::Compute,
        [&](ReprojectPassData& data, RenderGraphBuilder& builder)
        {
            data.indirectArgs = builder.ReadIndirectArg(indirectArgs);
            data.tileListBuffer = builder.Read(tileListBuffer);
            data.input = builder.Read(input);

            RenderGraphTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::R11G11B10F;
            data.output = builder.Create<RenderGraphTexture>(desc, "ReflectionDenoiser reproject output");
            data.output = builder.Write(data.output);
        },
        [=](const ReprojectPassData& data, IGfxCommandList* pCommandList)
        {
            RenderGraphTexture* input = (RenderGraphTexture*)pRenderGraph->GetResource(data.input);
            RenderGraphTexture* output = (RenderGraphTexture*)pRenderGraph->GetResource(data.output);
            RenderGraphBuffer* indirectArgs = (RenderGraphBuffer*)pRenderGraph->GetResource(data.indirectArgs);
            RenderGraphBuffer* tileListBuffer = (RenderGraphBuffer*)pRenderGraph->GetResource(data.tileListBuffer);

            pCommandList->SetPipelineState(m_pReprojectPSO);

            uint32_t constants[3] = { tileListBuffer->GetSRV()->GetHeapIndex(), input->GetSRV()->GetHeapIndex(), output->GetUAV()->GetHeapIndex() };
            pCommandList->SetComputeConstants(0, constants, sizeof(constants));
            pCommandList->DispatchIndirect(indirectArgs->GetBuffer(), 0);
        });

    return reproject_pass->output;
}

void ReflectionDenoiser::Reproject(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::Prefilter(IGfxCommandList* pCommandList)
{
}

void ReflectionDenoiser::ResolveTemporal(IGfxCommandList* pCommandList)
{
}
