#include "gi_denoiser_nrd.h"
#include "nrd_integration.h"
#include "utils/gui_util.h"
#include "../renderer.h"

static const nrd::Method NRDMethod = nrd::Method::REBLUR_DIFFUSE_SH;

GIDenoiserNRD::GIDenoiserNRD(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pReblur = eastl::make_unique<NRDIntegration>(pRenderer);
    m_pReblurSettings = eastl::make_unique<nrd::ReblurSettings>();
    m_pReblurSettings->antilagIntensitySettings.enable = true;
    m_pReblurSettings->antilagIntensitySettings.sensitivityToDarkness = 0.01f;

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_normal_roughness", "cs_6_6", {});
    m_pPackNormalRoughnessPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack normal/roughness PSO");

    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_radiance_hitT", "cs_6_6", {});
    m_pPackRadianceHitTPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack radiance/hitT PSO");

    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_velocity", "cs_6_6", {});
    m_pPackVelocityPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack velocity PSO");

    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "resolve_output", "cs_6_6", {});
    m_pResolveOutputPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - resolve PSO");
}

GIDenoiserNRD::~GIDenoiserNRD() = default;

void GIDenoiserNRD::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pResolvedOutput == nullptr ||
        m_pResolvedOutput->GetTexture()->GetDesc().width != width ||
        m_pResolvedOutput->GetTexture()->GetDesc().height != height ||
        m_bNeedCreateReblur)
    {
        m_pOutputSH0.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReBLUR - output SH0"));
        m_pOutputSH1.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReBLUR - output SH1"));
        m_pResolvedOutput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReBLUR - resolved output"));

        m_bHistoryInvalid = true;
        m_bNeedCreateReblur = true;
    }

    m_historyIrradiance = pRenderGraph->Import(m_pResolvedOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiserNRD::Render(RenderGraph* pRenderGraph, RGHandle radiance, RGHandle rayDirection, RGHandle normal, RGHandle linearDepth, RGHandle velocity, uint32_t width, uint32_t height)
{
    CreateReblurDenoiser(width, height);

    struct PackNormalRoughnessData
    {
        RGHandle normal;
        RGHandle packedNormal;
    };

    auto pack_normal_pass = pRenderGraph->AddPass<PackNormalRoughnessData>("ReBLUR - pack normal/roughness", RenderPassType::Compute,
        [&](PackNormalRoughnessData& data, RGBuilder& builder)
        {
            data.normal = builder.Read(normal);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGB10A2UNORM;
            data.packedNormal = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - packed normal"));
        },
        [=](const PackNormalRoughnessData& data, IGfxCommandList* pCommandList)
        {
            PackNormalRoughness(pCommandList, 
                pRenderGraph->GetTexture(data.normal), 
                pRenderGraph->GetTexture(data.packedNormal),
                width, height);
        });

    struct PackRadianceHitTData
    {
        RGHandle radiance;
        RGHandle rayDirection;
        RGHandle linearDepth;
        RGHandle outputSH0;
        RGHandle outputSH1;
    };

    auto pack_radiance_pass = pRenderGraph->AddPass<PackRadianceHitTData>("ReBLUR - pack radiance/hitT", RenderPassType::Compute,
        [&](PackRadianceHitTData& data, RGBuilder& builder)
        {
            data.radiance = builder.Read(radiance);
            data.rayDirection = builder.Read(rayDirection);
            data.linearDepth = builder.Read(linearDepth);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.outputSH0 = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - SH0"));
            data.outputSH1 = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - SH1"));
        },
        [=](const PackRadianceHitTData& data, IGfxCommandList* pCommandList)
        {
            PackRadiance(pCommandList, 
                pRenderGraph->GetTexture(data.radiance),
                pRenderGraph->GetTexture(data.rayDirection),
                pRenderGraph->GetTexture(data.linearDepth),
                pRenderGraph->GetTexture(data.outputSH0),
                pRenderGraph->GetTexture(data.outputSH1),
                width, height);
        });

    struct PackVelocityData
    {
        RGHandle velocity;
        RGHandle output;
    };

    auto pack_velocity_pass = pRenderGraph->AddPass<PackVelocityData>("", RenderPassType::Compute,
        [&](PackVelocityData& data, RGBuilder& builder)
        {
            data.velocity = builder.Read(velocity);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - velocity"));
        },
        [=](const PackVelocityData& data, IGfxCommandList* pCommandList)
        {
            PackVelocity(pCommandList,
                pRenderGraph->GetTexture(data.velocity),
                pRenderGraph->GetTexture(data.output),
                width, height);
        });

    struct ReblurData
    {
        RGHandle sh0;
        RGHandle sh1;
        RGHandle normal;
        RGHandle linearDepth;
        RGHandle velocity;
        RGHandle outputSH0;
        RGHandle outputSH1;
    };

    auto reblur_pass = pRenderGraph->AddPass<ReblurData>("ReBLUR", RenderPassType::Compute,
        [&](ReblurData& data, RGBuilder& builder)
        {
            data.sh0 = builder.Read(pack_radiance_pass->outputSH0);
            data.sh1 = builder.Read(pack_radiance_pass->outputSH1);
            data.normal = builder.Read(pack_normal_pass->packedNormal);
            data.linearDepth = builder.Read(linearDepth);
            data.velocity = builder.Read(pack_velocity_pass->output);
            data.outputSH0 = builder.Write(builder.Import(m_pOutputSH0->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
            data.outputSH1 = builder.Write(builder.Import(m_pOutputSH1->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
        },
        [=](const ReblurData& data, IGfxCommandList* pCommandList)
        {
            RGTexture* sh0 = pRenderGraph->GetTexture(data.sh0);
            RGTexture* sh1 = pRenderGraph->GetTexture(data.sh1);
            RGTexture* normal = pRenderGraph->GetTexture(data.normal);
            RGTexture* linearDepth = pRenderGraph->GetTexture(data.linearDepth);
            RGTexture* velocity = pRenderGraph->GetTexture(data.velocity);

            NRDUserPool userPool = {};
            userPool[(size_t)nrd::ResourceType::IN_MV] = { velocity->GetTexture(), velocity->GetSRV(), velocity->GetUAV(), GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_NORMAL_ROUGHNESS] = { normal->GetTexture(), normal->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_VIEWZ] = { linearDepth->GetTexture(), linearDepth->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_DIFF_SH0] = { sh0->GetTexture(), sh0->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_DIFF_SH1] = { sh1->GetTexture(), sh1->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::OUT_DIFF_SH0] = { m_pOutputSH0->GetTexture(), m_pOutputSH0->GetSRV(), m_pOutputSH0->GetUAV(), GfxResourceState::UnorderedAccess };
            userPool[(size_t)nrd::ResourceType::OUT_DIFF_SH1] = { m_pOutputSH1->GetTexture(), m_pOutputSH1->GetSRV(), m_pOutputSH1->GetUAV(), GfxResourceState::UnorderedAccess };

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            nrd::CommonSettings commonSettings = {};
            memcpy(commonSettings.viewToClipMatrix, &camera->GetNonJitterProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.viewToClipMatrixPrev, &camera->GetNonJitterPrevProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrix, &camera->GetViewMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrixPrev, &camera->GetPrevViewMatrix(), sizeof(float4x4));
            commonSettings.motionVectorScale[0] = -0.5f;
            commonSettings.motionVectorScale[1] = 0.5f;
            commonSettings.motionVectorScale[2] = 1.0f;
            commonSettings.cameraJitter[0] = camera->GetJitter().x;
            commonSettings.cameraJitter[1] = camera->GetJitter().y;
            commonSettings.frameIndex = (uint32_t)m_pRenderer->GetFrameID();
            commonSettings.accumulationMode = m_bHistoryInvalid ? nrd::AccumulationMode::RESTART : nrd::AccumulationMode::CONTINUE;
            commonSettings.isMotionVectorInWorldSpace = false;
            commonSettings.denoisingRange = camera->GetZFar();

            m_pReblur->SetMethodSettings(NRDMethod, m_pReblurSettings.get());
            m_pReblur->Denoise(pCommandList, commonSettings, userPool);
            m_bHistoryInvalid = false;

            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    struct ResolveData
    {
        RGHandle outputSH0;
        RGHandle outputSH1;
        RGHandle normal;
        RGHandle resolveOutput;
    };

    auto resolve_pass = pRenderGraph->AddPass<ResolveData>("ReBLUR - resolve", RenderPassType::Compute,
        [&](ResolveData& data, RGBuilder& builder)
        {
            data.outputSH0 = builder.Read(reblur_pass->outputSH0);
            data.outputSH1 = builder.Read(reblur_pass->outputSH1);
            data.normal = builder.Read(normal);
            data.resolveOutput = builder.Write(m_historyIrradiance);
        },
        [=](const ResolveData& data, IGfxCommandList* pCommandList)
        {
            ResolveOutput(pCommandList, pRenderGraph->GetTexture(data.normal), width, height);
        });

    return resolve_pass->resolveOutput;
}

void GIDenoiserNRD::CreateReblurDenoiser(uint32_t width, uint32_t height)
{
    if (m_bNeedCreateReblur)
    {
        m_pReblur->Destroy();

        nrd::MethodDesc methodDescs = { NRDMethod, (uint16_t)width, (uint16_t)height };
        m_pReblur->Initialize(methodDescs);

        m_bNeedCreateReblur = false;
    }
}

void GIDenoiserNRD::PackNormalRoughness(IGfxCommandList* pCommandList, RGTexture* normal, RGTexture* packedNormal, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPackNormalRoughnessPSO);

    uint32_t cb[2] = { normal->GetSRV()->GetHeapIndex(), packedNormal->GetUAV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void GIDenoiserNRD::PackRadiance(IGfxCommandList* pCommandList, RGTexture* radiance, RGTexture* rayDirection, RGTexture* linearDepth, RGTexture* outputSH0, RGTexture* outputSH1, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPackRadianceHitTPSO);

    struct CB
    {
        float4 hitDistParams;
        uint radianceTexture;
        uint rayDirectionTexture;
        uint linearDepthTexture;
        uint outputSH0Texture;
        uint outputSH1Texture;
    };

    CB cb;
    memcpy(&cb.hitDistParams, &m_pReblurSettings->hitDistanceParameters, sizeof(float4));
    cb.radianceTexture = radiance->GetSRV()->GetHeapIndex();
    cb.rayDirectionTexture = rayDirection->GetSRV()->GetHeapIndex();
    cb.linearDepthTexture = linearDepth->GetSRV()->GetHeapIndex();
    cb.outputSH0Texture = outputSH0->GetUAV()->GetHeapIndex();
    cb.outputSH1Texture = outputSH1->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(1, &cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void GIDenoiserNRD::PackVelocity(IGfxCommandList* pCommandList, RGTexture* velocity, RGTexture* output, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPackVelocityPSO);

    uint32_t cb[2] =
    {
        velocity->GetSRV()->GetHeapIndex(),
        output->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void GIDenoiserNRD::ResolveOutput(IGfxCommandList* pCommandList, RGTexture* normal, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolveOutputPSO);

    uint32_t cb[4] = { 
        m_pOutputSH0->GetSRV()->GetHeapIndex(), 
        m_pOutputSH1->GetSRV()->GetHeapIndex(),
        normal->GetSRV()->GetHeapIndex(),
        m_pResolvedOutput->GetUAV()->GetHeapIndex() 
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}