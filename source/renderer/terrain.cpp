#include "terrain.h"
#include "renderer.h"
#include "utils/gui_util.h"
#include "ImFileDialog/ImFileDialog.h"
#include "terrain/constants.hlsli"

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

    const uint32_t size = 2048;
    m_pHeightmap0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap0")); // 4 layers
    m_pHeightmap1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap1")); // 4 layers
    m_pWater.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Water"));
    m_pFlux.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "Flux"));
    m_pVelocity0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity0"));
    m_pVelocity1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity1"));
    m_pSediment0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment0"));
    m_pSediment1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment1"));
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

            eastl::swap(m_pSediment0, m_pSediment1);
            eastl::swap(m_pHeightmap0, m_pHeightmap1);
        });

    return raymarching_pass->output;
}

void Terrain::Heightmap(IGfxCommandList* pCommandList)
{
    static bool bGenerateHeightmap = true;
    static bool bInputTexture = false;
    static uint seed = 556650;

    bGenerateHeightmap |= ImGui::SliderInt("Seed##Terrain", (int*)&seed, 0, 1000000);
    bGenerateHeightmap |= ImGui::Button("Generate##Terrain");

    ImGui::SameLine();
    if (ImGui::Button("Load##Terrain"))
    {
        ifd::FileDialog::Instance().Open("LoadHeightmap", "Open Heightmap", "heightmap file (*.png){.png},.*");
    }

    if (bGenerateHeightmap || bInputTexture)
    {
        uint32_t width = m_pHeightmap0->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap0->GetTexture()->GetDesc().height;

        pCommandList->SetPipelineState(m_pHeightmapPSO);
        uint32_t cb[] = {
            seed,
            bInputTexture ? m_pInputTexture->GetSRV()->GetHeapIndex() : -1,
            m_pHeightmap0->GetUAV()->GetHeapIndex() };
        pCommandList->SetComputeConstants(0, cb, sizeof(cb));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);

        float clear_value[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pSediment0->GetTexture(), m_pSediment0->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pSediment1->GetTexture(), m_pSediment1->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pWater->GetTexture(), m_pWater->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pFlux->GetTexture(), m_pFlux->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pVelocity0->GetTexture(), m_pVelocity0->GetUAV(), clear_value);
        pCommandList->ClearUAV(m_pVelocity1->GetTexture(), m_pVelocity1->GetUAV(), clear_value);
        pCommandList->UavBarrier(nullptr);

        bGenerateHeightmap = false;
        bInputTexture = false;
    }

    if (ifd::FileDialog::Instance().IsDone("LoadHeightmap"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            eastl::string result = ifd::FileDialog::Instance().GetResult().u8string().c_str();
            m_pInputTexture.reset(m_pRenderer->CreateTexture2D(result, false));
            bInputTexture = true;
        }

        ifd::FileDialog::Instance().Close();
    }
}

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bRain = false;

    static float rainRate = 0.0001;
    static float evaporationRate = 0.05;
    static float4 erosionConstant = float4(0.2f, 0.2f, 0.2f, 0.2f);
    static float depositionConstant = 0.3f;
    static float sedimentCapacityConstant = 0.001f;
    static float smoothness = 1.0f;

    ImGui::Separator();
    ImGui::Checkbox("Rain##Terrain", &bRain);
    ImGui::SliderFloat("Rain Rate##Terrain", &rainRate, 0.0, 0.0005, "%.4f");
    ImGui::SliderFloat("Evaporation##Terrain", &evaporationRate, 0.0, 10.0, "%.2f");
    ImGui::SliderFloat4("Erosion##Terrain", (float*)&erosionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Deposition##Terrain", &depositionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Sediment Capacity##Terrain", &sedimentCapacityConstant, 0.0001, 0.005, "%.4f");
    ImGui::SliderFloat("Smoothness##Terrain", &smoothness, 0.0, 1.0, "%.2f");

    pCommandList->SetPipelineState(m_pErosionPSO);

    ErosionConstants cb;
    cb.c_heightmapUAV0 = m_pHeightmap0->GetUAV()->GetHeapIndex();
    cb.c_heightmapUAV1 = m_pHeightmap1->GetUAV()->GetHeapIndex();
    cb.c_sedimentUAV0 = m_pSediment0->GetUAV()->GetHeapIndex();
    cb.c_sedimentUAV1 = m_pSediment1->GetUAV()->GetHeapIndex();
    cb.c_waterUAV = m_pWater->GetUAV()->GetHeapIndex();
    cb.c_fluxUAV = m_pFlux->GetUAV()->GetHeapIndex();
    cb.c_velocityUAV0 = m_pVelocity0->GetUAV()->GetHeapIndex();
    cb.c_velocityUAV1 = m_pVelocity1->GetUAV()->GetHeapIndex();
    cb.c_bRain = bRain;
    cb.c_rainRate = rainRate;
    cb.c_evaporationRate = evaporationRate;
    cb.c_erosionConstant = erosionConstant;
    cb.c_depositionConstant = depositionConstant;
    cb.c_sedimentCapacityConstant = sedimentCapacityConstant * 0.05;
    cb.c_smoothness = smoothness;
        
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    uint32_t width = m_pHeightmap0->GetTexture()->GetDesc().width;
    uint32_t height = m_pHeightmap0->GetTexture()->GetDesc().height;

    for (uint32_t i = 0; i < 9; ++i)
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
        m_pHeightmap1->GetSRV()->GetHeapIndex(),
        m_pWater->GetSRV()->GetHeapIndex(),
        m_pFlux->GetSRV()->GetHeapIndex(),
        m_pVelocity0->GetSRV()->GetHeapIndex(),
        m_pSediment1->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(), 
        bShowLayers 
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
