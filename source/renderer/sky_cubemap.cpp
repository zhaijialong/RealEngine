#include "sky_cubemap.h"
#include "renderer.h"
#include "core/engine.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

SkyCubeMap::SkyCubeMap(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("sky_cubemap.hlsl", "main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(desc, "SkyCubeMap PSO");

    desc.cs = pRenderer->GetShader("cubemap_spd.hlsl", "main", "cs_6_6", {});
    m_pGenerateMipsPSO = pRenderer->GetPipelineState(desc, "Cubemap SPD PSO");

    desc.cs = pRenderer->GetShader("ibl_prefilter.hlsl", "specular_filter", "cs_6_6", {});
    m_pSpecularFilterPSO = pRenderer->GetPipelineState(desc, "IBL specular PSO");

    desc.cs = pRenderer->GetShader("ibl_prefilter.hlsl", "diffuse_filter", "cs_6_6", {});
    m_pDiffuseFilterPSO = pRenderer->GetPipelineState(desc, "IBL diffuse PSO");

    m_pSPDCounterBuffer.reset(pRenderer->CreateTypedBuffer(nullptr, GfxFormat::R32UI, 1, "SkyCubeMap::m_pSPDCounterBuffer", GfxMemoryType::GpuOnly, true));
    m_pTexture.reset(pRenderer->CreateTextureCube(128, 128, 8, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "SkyCubeMap::m_pTexture"));
    m_pSpecularTexture.reset(pRenderer->CreateTextureCube(128, 128, 8, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "SkyCubeMap::m_pSpecularTexture"));
    m_pDiffuseTexture.reset(pRenderer->CreateTextureCube(128, 128, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "SkyCubeMap::m_pDiffuseTexture"));
}

void SkyCubeMap::Update(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "SkyCubeMap");

    ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
    if (m_prevLightDir != light->GetLightDirection() ||
        m_prevLightColor != light->GetLightColor() ||
        m_prevLightIntensity != light->GetLightIntensity() || 1)
    {
        m_prevLightDir = light->GetLightDirection();
        m_prevLightColor = light->GetLightColor();
        m_prevLightIntensity = light->GetLightIntensity();

        UpdateCubeTexture(pCommandList);
        UpdateSpecularCubeTexture(pCommandList);
        UpdateDiffuseCubeTexture(pCommandList);
    }
}

void SkyCubeMap::UpdateCubeTexture(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "Generate SkyCubeMap");

    bool first_frame = m_pRenderer->GetFrameID() == 0;
    if (!first_frame)
    {
        pCommandList->ResourceBarrier(m_pTexture->GetTexture(), GFX_ALL_SUB_RESOURCE, GfxResourceState::ShaderResourceAll, GfxResourceState::UnorderedAccess);
    }

    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::UnorderedAccess, GfxResourceState::CopyDst);

    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint cubeTextureUAV;
        uint cubeTextureSize;
        float rcpTextureSize;
    };

    uint32_t size = m_pTexture->GetTexture()->GetDesc().width;

    CB cb = { m_pTexture->GetUAV(0)->GetHeapIndex(), size, 1.0f / size };
    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
    pCommandList->Dispatch((size + 7) / 8, (size + 7) / 8, 6);

    //reset spd counter(overlap with the above dispatch)
    pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);

    const GfxTextureDesc& textureDesc = m_pTexture->GetTexture()->GetDesc();
    pCommandList->ResourceBarrier(m_pSPDCounterBuffer->GetBuffer(), 0, GfxResourceState::CopyDst, GfxResourceState::UnorderedAccess);

    for (uint32_t slice = 0; slice < 6; ++slice)
    {
        uint32_t subresource = CalcSubresource(textureDesc, 0, slice);
        pCommandList->ResourceBarrier(m_pTexture->GetTexture(), subresource, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceAll);
    }

    //generate mipmaps, needed in specular filtering
    pCommandList->SetPipelineState(m_pGenerateMipsPSO);

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, size, size); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, textureDesc.mip_levels - 1);

    struct spdConstants
    {
        uint mips;
        uint numWorkGroups;
        uint2 workGroupOffset;

        float2 invInputSize;
        uint c_imgSrc;
        uint c_spdGlobalAtomicUAV;

        //hlsl packing rules : every element in an array is stored in a four-component vector
        uint4 c_imgDst[12]; // do no access MIP [5]
    };

    spdConstants constants = {};
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.mips = numWorkGroupsAndMips[1];
    constants.workGroupOffset[0] = workGroupOffset[0];
    constants.workGroupOffset[1] = workGroupOffset[1];
    constants.invInputSize[0] = 1.0f / size;
    constants.invInputSize[1] = 1.0f / size;

    constants.c_imgSrc = m_pTexture->GetArraySRV()->GetHeapIndex();
    constants.c_spdGlobalAtomicUAV = m_pSPDCounterBuffer->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < textureDesc.mip_levels - 1; ++i)
    {
        constants.c_imgDst[i].x = m_pTexture->GetUAV(i + 1)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 6; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    for (uint32_t slice = 0; slice < 6; ++slice)
    {
        for (uint32_t mip = 1; mip < textureDesc.mip_levels; ++mip)
        {
            uint32_t subresource = CalcSubresource(textureDesc, mip, slice);
            pCommandList->ResourceBarrier(m_pTexture->GetTexture(), subresource, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceAll);
        }
    }
}

void SkyCubeMap::UpdateSpecularCubeTexture(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "IBL Specular filter");

    bool first_frame = m_pRenderer->GetFrameID() == 0;
    if (!first_frame)
    {
        pCommandList->ResourceBarrier(m_pSpecularTexture->GetTexture(), GFX_ALL_SUB_RESOURCE, GfxResourceState::ShaderResourceAll, GfxResourceState::UnorderedAccess);
    }

    pCommandList->SetPipelineState(m_pSpecularFilterPSO);

    const GfxTextureDesc& textureDesc = m_pSpecularTexture->GetTexture()->GetDesc();
    for (uint32_t i = 0; i < textureDesc.mip_levels; ++i)
    {
        uint32_t cb[5] = {
            i == 0 ? m_pTexture->GetArraySRV()->GetHeapIndex() : m_pTexture->GetSRV()->GetHeapIndex(),
            m_pSpecularTexture->GetUAV(i)->GetHeapIndex(), 
            textureDesc.width,
            i,
            textureDesc.mip_levels
        };
        pCommandList->SetComputeConstants(0, cb, sizeof(cb));

        uint32_t size = textureDesc.width >> i;
        pCommandList->Dispatch((size + 7) / 8, (size + 7) / 8, 6);
    }

    pCommandList->ResourceBarrier(m_pSpecularTexture->GetTexture(), GFX_ALL_SUB_RESOURCE, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceAll);
}

void SkyCubeMap::UpdateDiffuseCubeTexture(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "IBL diffuse filter");
    //todo
}
