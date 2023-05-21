#include "terrain.h"
#include "renderer.h"
#include "utils/gui_util.h"

Terrain::Terrain(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("terrain/terrain.hlsl", "main", "cs_6_6", {});
    m_pRaymachPSO = pRenderer->GetPipelineState(desc, "terrain raymarching PSO");

    desc.cs = pRenderer->GetShader("terrain/heightmap.hlsl", "main", "cs_6_6", {});
    m_pHeightmapPSO = pRenderer->GetPipelineState(desc, "terrain heightmap PSO");

    desc.cs = pRenderer->GetShader("terrain/erosion.hlsl", "main", "cs_6_6", {});
    m_pErosionPSO = pRenderer->GetPipelineState(desc, "terrain erosion PSO");

    m_pHeightmap.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap"));
    m_pHardness.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Hardness"));
    m_pSediment.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Sediment"));
    m_pWater.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Water"));
    m_pFlux.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "Flux"));
    m_pVelocity.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::RG16UNORM, GfxTextureUsageUnorderedAccess, "Velocity"));
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
            ImGui::Begin("Terrain");
            Heightmap(pCommandList);
            Erosion(pCommandList);
            Raymarch(pCommandList, pRenderGraph->GetTexture(data.output));
            ImGui::End();
        });

    return raymarching_pass->output;
}

void Terrain::Heightmap(IGfxCommandList* pCommandList)
{
    static bool bGenerateHeightmap = true;
    static uint seed = 556650;

    bGenerateHeightmap |= ImGui::SliderInt("Seed##Terrain", (int*)&seed, 0, 1000000);
    bGenerateHeightmap |= ImGui::Button("Generate##Terrain");
    ImGui::Image((ImTextureID)m_pHeightmap->GetSRV(), ImVec2(300, 300));

    if (bGenerateHeightmap)
    {
        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        pCommandList->SetPipelineState(m_pHeightmapPSO);
        uint32_t cb[] = { seed, m_pHeightmap->GetUAV()->GetHeapIndex() };
        pCommandList->SetComputeConstants(0, cb, sizeof(cb));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);

        bGenerateHeightmap = false;
    }
}

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bErosion = false;

    ImGui::Checkbox("Erosion##Terrain", &bErosion);

    if (bErosion)
    {
        pCommandList->SetPipelineState(m_pErosionPSO);

        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        for (uint32_t i = 0; i < 5; ++i)
        {
            pCommandList->SetComputeConstants(0, &i, sizeof(uint32_t));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
            pCommandList->UavBarrier(nullptr);
        }
    }
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
