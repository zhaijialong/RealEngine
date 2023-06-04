#include "terrain.h"
#include "renderer.h"
#include "utils/gui_util.h"
#include "ImFileDialog/ImFileDialog.h"
#include "terrain/constants.hlsli"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "lodepng.h"

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

    desc.cs = pRenderer->GetShader("terrain/hardness.hlsl", "main", "cs_6_6", {});
    m_pHardnessPSO = pRenderer->GetPipelineState(desc, "terrain hardness PSO");

    desc.cs = pRenderer->GetShader("terrain/hardness.hlsl", "blur", "cs_6_6", {});
    m_pBlurPSO = pRenderer->GetPipelineState(desc, "hardness blur PSO");


    const uint32_t size = 1024;
    m_pHeightmap.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap"));
    m_pWater.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Water"));
    m_pFlux.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "Flux"));
    m_pVelocity.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity"));
    m_pSediment0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment0"));
    m_pSediment1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment1"));
    m_pSoilFlux.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA32UI, GfxTextureUsageUnorderedAccess, "SoilFlux"));
    m_pHardness.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Hardness"));
    m_pHardnessBlurTemp.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "HardnessTemp"));

    GfxBufferDesc bufferDesc;
    bufferDesc.size = m_pHeightmap->GetTexture()->GetRowPitch(0) * size;
    bufferDesc.memory_type = GfxMemoryType::GpuToCpu;
    m_pStagingBuffer.reset(pRenderer->GetDevice()->CreateBuffer(bufferDesc, "Heightmap readback"));
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
            Hardness(pCommandList);
            Erosion(pCommandList);
            Raymarch(pCommandList, pRenderGraph->GetTexture(data.output));
            ImGui::End();

            eastl::swap(m_pSediment0, m_pSediment1);
        });

    return raymarching_pass->output;
}

void Terrain::Heightmap(IGfxCommandList* pCommandList)
{
    static bool bGenerateHeightmap = true;
    static bool bInputTexture = false;
    static bool bSaveTexture = false;
    static uint seed = 556650;

    bGenerateHeightmap |= ImGui::SliderInt("Seed##Terrain", (int*)&seed, 0, 1000000);
    bGenerateHeightmap |= ImGui::Button("Generate##Terrain");

    ImGui::SameLine();
    if (ImGui::Button("Load##Terrain"))
    {
        ifd::FileDialog::Instance().Open("LoadHeightmap", "Open Heightmap", "heightmap file (*.png){.png},.*");
    }

    ImGui::SameLine();
    if (ImGui::Button("Save##Terrain"))
    {
        ifd::FileDialog::Instance().Save("SaveHeightmap", "Save Heightmap", "heightmap file (*.png){.png},.*");
    }

    if (bGenerateHeightmap || bInputTexture)
    {
        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        pCommandList->SetPipelineState(m_pHeightmapPSO);
        uint32_t cb[] = {
            seed,
            bInputTexture ? m_pInputTexture->GetSRV()->GetHeapIndex() : -1,
            m_pHeightmap->GetUAV()->GetHeapIndex() };
        pCommandList->SetComputeConstants(0, cb, sizeof(cb));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);

        Clear(pCommandList);
        bGenerateHeightmap = false;
        bInputTexture = false;
    }

    if (bSaveTexture)
    {
        Save();
        bSaveTexture = false;
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

    if (ifd::FileDialog::Instance().IsDone("SaveHeightmap"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            pCommandList->ResourceBarrier(m_pHeightmap->GetTexture(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopySrc);
            pCommandList->CopyTextureToBuffer(m_pStagingBuffer.get(), m_pHeightmap->GetTexture(), 0, 0);
            pCommandList->ResourceBarrier(m_pHeightmap->GetTexture(), 0, GfxResourceState::CopySrc, GfxResourceState::UnorderedAccess);
            m_savePath = ifd::FileDialog::Instance().GetResult().u8string().c_str();
            bSaveTexture = true;
        }

        ifd::FileDialog::Instance().Close();
    }
}

void Terrain::Hardness(IGfxCommandList* pCommandList)
{
    static bool bGenerateHardness = true;
    static uint32_t seed = 295678;
    static float minHardness = 0.2;
    static float maxHardness = 1.0;

    ImGui::Separator();
    bGenerateHardness |= ImGui::SliderInt("Hardness Seed##Terrain", (int*)&seed, 0, 1000000);
    bGenerateHardness |= ImGui::SliderFloat("Hardness Min##Terrain", &minHardness, 0.0, 0.5);
    bGenerateHardness |= ImGui::SliderFloat("Hardness Max##Terrain", &maxHardness, 0.5, 1.0);

    if (bGenerateHardness)
    {
        pCommandList->BeginEvent("hardness");
        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        struct HardnessConstants
        {
            uint c_hardnessSeed;
            uint c_heightmap;
            uint c_hardnessUAV;
            float c_minHardness;
            float c_maxHardness;
        };
        HardnessConstants cb;
        cb.c_hardnessSeed = seed;
        cb.c_heightmap = m_pHeightmap->GetSRV()->GetHeapIndex();
        cb.c_hardnessUAV = m_pHardness->GetUAV()->GetHeapIndex();
        cb.c_minHardness = minHardness;
        cb.c_maxHardness = maxHardness;

        pCommandList->SetPipelineState(m_pHardnessPSO);
        pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);

        pCommandList->SetPipelineState(m_pBlurPSO);
        uint32_t cb1[] = { m_pHardness->GetSRV()->GetHeapIndex(), m_pHardnessBlurTemp->GetUAV()->GetHeapIndex(), 1 };
        pCommandList->SetComputeConstants(0, cb1, sizeof(cb1));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);

        uint32_t cb2[] = { m_pHardnessBlurTemp->GetSRV()->GetHeapIndex(), m_pHardness->GetUAV()->GetHeapIndex(), 0 };
        pCommandList->SetComputeConstants(0, cb2, sizeof(cb2));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);

        bGenerateHardness = false;
        pCommandList->EndEvent();
    }
}

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bErosion = false;

    static float rainRate = 0.0001;
    static float evaporationRate = 0.05;
    static float erosionConstant = 0.3f;
    static float depositionConstant = 0.05f;
    static float sedimentCapacityConstant = 0.00005f;
    static float sedimentSofteningConstant = 0.01f;
    static float thermalErosionConstant = 0.3f;
    static float talusAngleTan = tan(degree_to_radian(30.0f));
    static float talusAngleBias = 0.1f;

    ImGui::Separator();
    if (ImGui::Checkbox("Enable Erosion##Terrain", &bErosion))
    {
        if (!bErosion)
        {
            Clear(pCommandList);
        }
    }
    ImGui::SliderFloat("Rain Rate##Terrain", &rainRate, 0.0, 0.0005, "%.4f");
    ImGui::SliderFloat("Evaporation##Terrain", &evaporationRate, 0.0, 10.0, "%.2f");
    ImGui::SliderFloat("Erosion##Terrain", (float*)&erosionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Deposition##Terrain", &depositionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Sediment Capacity##Terrain", &sedimentCapacityConstant, 0.00000, 0.0001, "%.5f");
    ImGui::SliderFloat("Sediment Softening##Terrain", &sedimentSofteningConstant, 0.0, 0.03, "%.4f");
    ImGui::SliderFloat("Thermal Erosion##Terrain", &thermalErosionConstant, 0.0, 5.0, "%.2f");
    ImGui::SliderFloat("Talus Angle Tan##Terrain", &talusAngleTan, 0.0, 1.0, "%.2f");
    ImGui::SliderFloat("Talus Angle Bias##Terrain", &talusAngleBias, 0.0, 0.3, "%.2f");

    if (bErosion)
    {
        pCommandList->SetPipelineState(m_pErosionPSO);

        ErosionConstants cb;
        cb.c_heightmapUAV = m_pHeightmap->GetUAV()->GetHeapIndex();
        cb.c_sedimentUAV0 = m_pSediment0->GetUAV()->GetHeapIndex();
        cb.c_sedimentUAV1 = m_pSediment1->GetUAV()->GetHeapIndex();
        cb.c_waterUAV = m_pWater->GetUAV()->GetHeapIndex();
        cb.c_fluxUAV = m_pFlux->GetUAV()->GetHeapIndex();
        cb.c_velocityUAV = m_pVelocity->GetUAV()->GetHeapIndex();
        cb.c_soilFluxUAV = m_pSoilFlux->GetUAV()->GetHeapIndex();
        cb.c_hardnessUAV = m_pHardness->GetUAV()->GetHeapIndex();
        cb.c_rainRate = rainRate;
        cb.c_evaporationRate = evaporationRate;
        cb.c_erosionConstant = erosionConstant;
        cb.c_depositionConstant = depositionConstant;
        cb.c_sedimentCapacityConstant = sedimentCapacityConstant;
        cb.c_sedimentSofteningConstant = sedimentSofteningConstant;
        cb.c_thermalErosionConstant = thermalErosionConstant;
        cb.c_talusAngleTan = talusAngleTan;
        cb.c_talusAngleBias = talusAngleBias;

        pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        const char* pass_names[] =
        {
            "rain",
            "flow",
            "update_water",
            "thermal_flow",
            "force_based_erosion",
            "advect_sediment",
            "thermal_erosion",
            "evaporation",
        };

        for (uint32_t i = 0; i < 8; ++i)
        {
            pCommandList->BeginEvent(pass_names[i]);
            pCommandList->SetComputeConstants(0, &i, sizeof(uint32_t));
            pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
            pCommandList->UavBarrier(nullptr);
            pCommandList->EndEvent();
        }
    }
}

void Terrain::Raymarch(IGfxCommandList* pCommandList, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pRaymachPSO);

    uint32_t cb[] = {
        m_pHeightmap->GetSRV()->GetHeapIndex(),
        m_pWater->GetSRV()->GetHeapIndex(),
        m_pFlux->GetSRV()->GetHeapIndex(),
        m_pVelocity->GetSRV()->GetHeapIndex(),
        m_pSediment1->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(), 
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void Terrain::Clear(IGfxCommandList* pCommandList)
{
    float clear_value[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCommandList->ClearUAV(m_pSediment0->GetTexture(), m_pSediment0->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pSediment1->GetTexture(), m_pSediment1->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pWater->GetTexture(), m_pWater->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pFlux->GetTexture(), m_pFlux->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pVelocity->GetTexture(), m_pVelocity->GetUAV(), clear_value);
    pCommandList->UavBarrier(nullptr);
}

void Terrain::Save()
{
    m_pRenderer->WaitGpuFinished();

    uint8_t* readback_data = (uint8_t*)m_pStagingBuffer->GetCpuAddress();
    uint32_t readback_pitch = m_pHeightmap->GetTexture()->GetRowPitch(0);

    uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
    uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

    
    eastl::vector<uint16_t> data;
    data.resize(width * height);

    for (uint32_t i = 0; i < height; ++i)
    {
        memcpy((uint8_t*)data.data() + i * width * sizeof(uint16_t),
            readback_data + i * readback_pitch,
            width * sizeof(uint16_t));
    }

    // it does not support 16bits !!
    //stbi_write_png(m_savePath.c_str(), width, height, 1, data.data(), width * sizeof(uint16_t));

    lodepng::State state;
    state.info_raw.bitdepth = 16;
    state.info_raw.colortype = LCT_GREY;
    state.info_png.color.bitdepth = 16;
    state.info_png.color.colortype = LCT_GREY;
    state.encoder.auto_convert = 0;

    eastl::vector<uint16_t> data_big_endian;
    data_big_endian.reserve(data.size());

    for (uint32_t i = 0; i < data.size(); ++i)
    {
        uint16_t x = data[i];
        data_big_endian.push_back(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
    }

    std::vector<unsigned char> png;
    lodepng::encode(png, (const unsigned char*)data_big_endian.data(), width, height, state);
    lodepng::save_file(png, m_savePath.c_str());
}
