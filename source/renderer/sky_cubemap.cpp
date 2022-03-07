#include "sky_cubemap.h"
#include "renderer.h"
#include "core/engine.h"

SkyCubeMap::SkyCubeMap(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("sky_cubemap.hlsl", "main", "cs_6_6", {});
    m_pPSO = pRenderer->GetPipelineState(desc, "SkyCubeMap PSO");

    m_pTexture.reset(pRenderer->CreateTextureCube(256, 256, 1, GfxFormat::R11G11B10F, GfxTextureUsageUnorderedAccess, "SkyCubeMap::m_pTexture"));
}

void SkyCubeMap::Update(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "SkyCubeMap");

    ILight* light = Engine::GetInstance()->GetWorld()->GetPrimaryLight();
    if (m_prevLightDir != light->GetLightDirection() ||
        m_prevLightColor != light->GetLightColor() ||
        m_prevLightIntensity != light->GetLightIntensity())
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
    bool first_frame = m_pRenderer->GetFrameID() == 0;
    if (!first_frame)
    {
        pCommandList->ResourceBarrier(m_pTexture->GetTexture(), GFX_ALL_SUB_RESOURCE, GfxResourceState::ShaderResourceAll, GfxResourceState::UnorderedAccess);
    }

    pCommandList->SetPipelineState(m_pPSO);

    struct CB
    {
        uint cubeTextureUAV;
        uint cubeTextureSize;
        float rcpTextureSize;
    };

    uint32_t size = m_pTexture->GetTexture()->GetDesc().width;

    CB cb = { m_pTexture->GetUAV()->GetHeapIndex(), size, 1.0f / size };
    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));

    pCommandList->Dispatch((size + 7) / 8, (size + 7) / 8, 6);

    pCommandList->ResourceBarrier(m_pTexture->GetTexture(), GFX_ALL_SUB_RESOURCE, GfxResourceState::UnorderedAccess, GfxResourceState::ShaderResourceAll);
}

void SkyCubeMap::UpdateSpecularCubeTexture(IGfxCommandList* pCommandList)
{
    //todo
}

void SkyCubeMap::UpdateDiffuseCubeTexture(IGfxCommandList* pCommandList)
{
    //todo
}
