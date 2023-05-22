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

    m_pHeightmap.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap")); // 4 layers
    m_pSediment.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Sediment"));
    m_pWater.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Water"));
    m_pFlux.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "Flux"));
    m_pVelocity.reset(pRenderer->CreateTexture2D(1024, 1024, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity"));
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

    if (bGenerateHeightmap)
    {
        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        pCommandList->SetPipelineState(m_pHeightmapPSO);
        uint32_t cb[] = { seed, m_pHeightmap->GetUAV()->GetHeapIndex() };
        pCommandList->SetComputeConstants(0, cb, sizeof(cb));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);

        float clear_value[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pSediment->GetTexture(), m_pSediment->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pWater->GetTexture(), m_pWater->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pFlux->GetTexture(), m_pFlux->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pVelocity->GetTexture(), m_pVelocity->GetUAV(), clear_value);
        pCommandList->UavBarrier(nullptr);

        bGenerateHeightmap = false;
    }
}

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bRain = false;

    static float rainRate = 0.0001;
    static float evaporationRate = 0.005;
    static float4 sedimentCapacity = float4(0.0001f, 0.1f, 0.0f, 0.0f);
    static float erosionConstant = 0.3f;
    static float depositionConstant = 0.1f;

    ImGui::Separator();
    ImGui::Checkbox("Rain##Terrain", &bRain);
    ImGui::SliderFloat("Rain Rate##Terrain", &rainRate, 0.0001, 0.001, "%.4f");
    ImGui::SliderFloat("Evaporation##Terrain", &evaporationRate, 0.005, 0.05, "%.4f");
    ImGui::SliderFloat4("Sediment Capacity##Terrain", (float*)&sedimentCapacity, 0.0001, 0.1, "%.4f");
    ImGui::SliderFloat("Erosion##Terrain", &erosionConstant, 0.001, 0.5, "%.4f");
    ImGui::SliderFloat("Deposition##Terrain", &depositionConstant, 0.001, 0.5, "%.4f");

    pCommandList->SetPipelineState(m_pErosionPSO);

    struct
    {
        uint c_heightmapUAV;
        uint c_sedimentUAV;
        uint c_waterUAV;
        uint c_fluxUAV;

        uint c_velocityUAV;
        uint c_bRain;
        float c_rainRate;
        float c_evaporationRate;

        float4 c_sedimentCapacity;
        float c_erosionConstant;
        float c_depositionConstant;
    } cb;
    cb.c_heightmapUAV = m_pHeightmap->GetUAV()->GetHeapIndex();
    cb.c_sedimentUAV = m_pSediment->GetUAV()->GetHeapIndex();
    cb.c_waterUAV = m_pWater->GetUAV()->GetHeapIndex();
    cb.c_fluxUAV = m_pFlux->GetUAV()->GetHeapIndex();
    cb.c_velocityUAV = m_pVelocity->GetUAV()->GetHeapIndex();
    cb.c_bRain = bRain;
    cb.c_rainRate = rainRate;
    cb.c_evaporationRate = evaporationRate;
    cb.c_sedimentCapacity = sedimentCapacity;
    cb.c_erosionConstant = erosionConstant;
    cb.c_depositionConstant = depositionConstant;
        
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
    uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

    for (uint32_t i = 0; i < 6; ++i)
    {
        pCommandList->SetComputeConstants(0, &i, sizeof(uint32_t));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);
    }
}

void Terrain::Raymarch(IGfxCommandList* pCommandList, RGTexture* output)
{
    static bool bShowLayers = true;

    ImGui::Separator();
    ImGui::Checkbox("Show Layers##Terrain", &bShowLayers);

    pCommandList->SetPipelineState(m_pRaymachPSO);

    uint32_t cb[] = {
        m_pHeightmap->GetSRV()->GetHeapIndex(),
        m_pWater->GetSRV()->GetHeapIndex(),
        m_pFlux->GetSRV()->GetHeapIndex(),
        m_pVelocity->GetSRV()->GetHeapIndex(),
        m_pSediment->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(), 
        bShowLayers 
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
