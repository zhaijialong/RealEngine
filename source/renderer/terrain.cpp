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

    const uint32_t size = 2048;
    m_pHeightmap0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap0"));
    m_pHeightmap1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap1"));
    m_pWater.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Water"));
    m_pFlux.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "Flux"));
    m_pVelocity0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity0"));
    m_pVelocity1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RG16F, GfxTextureUsageUnorderedAccess, "Velocity1"));
    m_pSediment0.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment0"));
    m_pSediment1.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Sediment1"));
    m_pRegolith.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "Regolith"));
    m_pRegolithFlux.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "RegolithFlux"));

    GfxBufferDesc bufferDesc;
    bufferDesc.size = m_pHeightmap0->GetTexture()->GetRowPitch(0) * size;
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
        uint32_t width = m_pHeightmap0->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap0->GetTexture()->GetDesc().height;

        pCommandList->SetPipelineState(m_pHeightmapPSO);
        uint32_t cb[] = {
            seed,
            bInputTexture ? m_pInputTexture->GetSRV()->GetHeapIndex() : -1,
            m_pHeightmap0->GetUAV()->GetHeapIndex() };
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
            pCommandList->ResourceBarrier(m_pHeightmap0->GetTexture(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopySrc);
            pCommandList->CopyTextureToBuffer(m_pStagingBuffer.get(), m_pHeightmap0->GetTexture(), 0, 0);
            pCommandList->ResourceBarrier(m_pHeightmap0->GetTexture(), 0, GfxResourceState::CopySrc, GfxResourceState::UnorderedAccess);
            m_savePath = ifd::FileDialog::Instance().GetResult().u8string().c_str();
            bSaveTexture = true;
        }

        ifd::FileDialog::Instance().Close();
    }
}

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bRain = false;

    static float rainRate = 0.0001;
    static float evaporationRate = 0.05;
    static float erosionConstant = 0.3f;
    static float depositionConstant = 0.05f;
    static float sedimentCapacityConstant = 0.00005f;
    static float maxRegolith = 0.;
    static float smoothness = 1.0f;

    ImGui::Separator();
    if (ImGui::Checkbox("Rain##Terrain", &bRain))
    {
        if (!bRain)
        {
            Clear(pCommandList);
        }
    }
    ImGui::SliderFloat("Rain Rate##Terrain", &rainRate, 0.0, 0.0005, "%.4f");
    ImGui::SliderFloat("Evaporation##Terrain", &evaporationRate, 0.0, 10.0, "%.2f");
    ImGui::SliderFloat("Erosion##Terrain", (float*)&erosionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Deposition##Terrain", &depositionConstant, 0.001, 0.5, "%.2f");
    ImGui::SliderFloat("Sediment Capacity##Terrain", &sedimentCapacityConstant, 0.00000, 0.0001, "%.5f");
    ImGui::SliderFloat("Max Regolith##Terrain", &maxRegolith, 0.00000, 0.01, "%.5f");
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
    cb.c_regolithUAV = m_pRegolith->GetUAV()->GetHeapIndex();
    cb.c_regolithFluxUAV = m_pRegolithFlux->GetUAV()->GetHeapIndex();
    cb.c_bRain = bRain;
    cb.c_rainRate = rainRate;
    cb.c_evaporationRate = evaporationRate;
    cb.c_erosionConstant = erosionConstant;
    cb.c_depositionConstant = depositionConstant;
    cb.c_sedimentCapacityConstant = sedimentCapacityConstant;
    cb.c_maxRegolith = maxRegolith;
    cb.c_smoothness = smoothness;
        
    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

    uint32_t width = m_pHeightmap0->GetTexture()->GetDesc().width;
    uint32_t height = m_pHeightmap0->GetTexture()->GetDesc().height;

    const char* pass_names[] =
    {
        "rain",
        "flow",
        "update_water",
        "diffuse_velocity0",
        "diffuse_velocity1",
        "force_based_erosion",
        "advect_sediment",
        "regolith_flow",
        "regolith_update",
        "dissolution_based_erosion",
        "evaporation",
        "smooth_height",
    };

    for (uint32_t i = 0; i < 12; ++i)
    {
        pCommandList->BeginEvent(pass_names[i]);
        pCommandList->SetComputeConstants(0, &i, sizeof(uint32_t));
        pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
        pCommandList->UavBarrier(nullptr);
        pCommandList->EndEvent();
    }
}

void Terrain::Raymarch(IGfxCommandList* pCommandList, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pRaymachPSO);

    uint32_t cb[] = {
        m_pHeightmap1->GetSRV()->GetHeapIndex(),
        m_pWater->GetSRV()->GetHeapIndex(),
        m_pFlux->GetSRV()->GetHeapIndex(),
        m_pVelocity0->GetSRV()->GetHeapIndex(),
        m_pSediment1->GetSRV()->GetHeapIndex(),
        m_pRegolith->GetSRV()->GetHeapIndex(),
        m_pRegolithFlux->GetSRV()->GetHeapIndex(),
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
    pCommandList->ClearUAV(m_pVelocity0->GetTexture(), m_pVelocity0->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pVelocity1->GetTexture(), m_pVelocity1->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pRegolith->GetTexture(), m_pRegolith->GetUAV(), clear_value);
    pCommandList->ClearUAV(m_pRegolithFlux->GetTexture(), m_pRegolithFlux->GetUAV(), clear_value);
    pCommandList->UavBarrier(nullptr);
}

void Terrain::Save()
{
    m_pRenderer->WaitGpuFinished();

    uint8_t* readback_data = (uint8_t*)m_pStagingBuffer->GetCpuAddress();
    uint32_t readback_pitch = m_pHeightmap0->GetTexture()->GetRowPitch(0);

    uint32_t width = m_pHeightmap0->GetTexture()->GetDesc().width;
    uint32_t height = m_pHeightmap0->GetTexture()->GetDesc().height;

    
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
