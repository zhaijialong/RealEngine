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

    const uint32_t size = 1024;
    m_pHeightmap.reset(pRenderer->CreateTexture2D(size, size, 1, GfxFormat::R16UNORM, GfxTextureUsageUnorderedAccess, "Heightmap0"));

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
            Erosion(pCommandList);
            Raymarch(pCommandList, pRenderGraph->GetTexture(data.output));
            ImGui::End();
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

void Terrain::Erosion(IGfxCommandList* pCommandList)
{
    static bool bErosion = false;
    static uint32_t cycles = 0;

    ImGui::Separator();
    if (ImGui::Checkbox("Erosion##Terrain", &bErosion))
    {
        cycles = 0;
    }

    if (bErosion)
    {
        if (cycles++ > 10)
        {
            bErosion = false;
        }

        ErosionConstants cb;
        cb.c_heightmapUAV = m_pHeightmap->GetUAV()->GetHeapIndex();

        pCommandList->SetPipelineState(m_pErosionPSO);
        pCommandList->SetComputeConstants(1, &cb, sizeof(cb));

        uint32_t width = m_pHeightmap->GetTexture()->GetDesc().width;
        uint32_t height = m_pHeightmap->GetTexture()->GetDesc().height;

        pCommandList->Dispatch(DivideRoudingUp(width, 32), DivideRoudingUp(height, 32), 1);
        pCommandList->UavBarrier(nullptr);
    }
}

void Terrain::Raymarch(IGfxCommandList* pCommandList, RGTexture* output)
{
    pCommandList->SetPipelineState(m_pRaymachPSO);

    uint32_t cb[] = {
        m_pHeightmap->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex(), 
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t width = output->GetTexture()->GetDesc().width;
    uint32_t height = output->GetTexture()->GetDesc().height;
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void Terrain::Clear(IGfxCommandList* pCommandList)
{

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
