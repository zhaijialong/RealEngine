#include "terrain.h"
#include "renderer.h"
#include "core/engine.h"

Terrain::Terrain(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("terrain/terrain.hlsl", "main", "cs_6_6", {});
    m_pRaymachPSO = pRenderer->GetPipelineState(desc, "terrain raymarching PSO");

    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pHeightmap.reset(pRenderer->CreateTexture2D(asset_path + "textures/Erosion.png", false));
}

RGHandle Terrain::Render(RenderGraph* pRenderGraph, RGHandle output)
{
    struct TerrainRaymarchingPassData
    {
        RGHandle output;
    };

    auto raymarching_pass = pRenderGraph->AddPass<TerrainRaymarchingPassData>("Terrain", RenderPassType::Compute,
        [&](TerrainRaymarchingPassData& data, RGBuilder& builder)
        {
            data.output = builder.Write(output);
        },
        [=](const TerrainRaymarchingPassData& data, IGfxCommandList* pCommandList)
        {
            Raymarch(pCommandList, pRenderGraph->GetTexture(data.output));
        });

    return raymarching_pass->output;
}

void Terrain::Raymarch(IGfxCommandList* pCommandList, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pRaymachPSO);

    uint32_t cb[] = { m_pHeightmap->GetSRV()->GetHeapIndex(), output->GetUAV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
