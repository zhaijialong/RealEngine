#include "gi_denoiser_nrd.h"
#include "nrd_integration.h"
#include "utils/gui_util.h"
#include "../renderer.h"

static const nrd::Method NRDMethod = nrd::Method::REBLUR_DIFFUSE; //todo : REBLUR_DIFFUSE_SH

GIDenoiserNRD::GIDenoiserNRD(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pReblur = eastl::make_unique<NRDIntegration>(pRenderer);
    m_pReblurSettings = eastl::make_unique<nrd::ReblurSettings>();

    Engine::GetInstance()->WindowResizeSignal.connect(&GIDenoiserNRD::OnWindowResize, this);

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_normal_roughness", "cs_6_6", {});
    m_pPackNormalRoughnessPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack normal/roughness PSO");

    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_radiance_hitT", "cs_6_6", {});
    m_pPackRadianceHitTPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack radiance/hitT PSO");

    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "resolve_output", "cs_6_6", {});
    m_pResolveOutputPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - resolve PSO");
}

GIDenoiserNRD::~GIDenoiserNRD() = default;

void GIDenoiserNRD::ImportHistoryTextures(RenderGraph* pRenderGraph, uint32_t width, uint32_t height)
{
    if (m_pOutputRadiance == nullptr ||
        m_pOutputRadiance->GetTexture()->GetDesc().width != width ||
        m_pOutputRadiance->GetTexture()->GetDesc().height != height ||
        m_bNeedCreateReblur)
    {
        m_pOutputRadiance.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReBLUR - output"));
        m_pResolvedOutput.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA16F, GfxTextureUsageUnorderedAccess, "ReBLUR - resolved output"));

        m_bHistoryInvalid = true;
    }

    m_historyIrradiance = pRenderGraph->Import(m_pResolvedOutput->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS);
}

RGHandle GIDenoiserNRD::Render(RenderGraph* pRenderGraph, RGHandle radiance, RGHandle normal, RGHandle linearDepth, RGHandle velocity, uint32_t width, uint32_t height)
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
        RGHandle linearDepth;
        RGHandle packedRadiance;
    };

    auto pack_radiance_pass = pRenderGraph->AddPass<PackRadianceHitTData>("ReBLUR - pack radiance/hitT", RenderPassType::Compute,
        [&](PackRadianceHitTData& data, RGBuilder& builder)
        {
            data.radiance = builder.Read(radiance);
            data.linearDepth = builder.Read(linearDepth);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.packedRadiance = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - packed radiance"));
        },
        [=](const PackRadianceHitTData& data, IGfxCommandList* pCommandList)
        {
            PackRadiance(pCommandList, 
                pRenderGraph->GetTexture(data.radiance),
                pRenderGraph->GetTexture(data.linearDepth),
                pRenderGraph->GetTexture(data.packedRadiance),
                width, height);
        });

    struct ReblurData
    {
        RGHandle radiance;
        RGHandle normal;
        RGHandle linearDepth;
        RGHandle velocity;
        RGHandle output;
    };

    auto reblur_pass = pRenderGraph->AddPass<ReblurData>("ReBLUR", RenderPassType::Compute,
        [&](ReblurData& data, RGBuilder& builder)
        {
            data.radiance = builder.Read(pack_radiance_pass->packedRadiance);
            data.normal = builder.Read(pack_normal_pass->packedNormal);
            data.linearDepth = builder.Read(linearDepth);
            data.velocity = builder.Read(velocity);
            data.output = builder.Write(builder.Import(m_pOutputRadiance->GetTexture(), m_bHistoryInvalid ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS));
        },
        [=](const ReblurData& data, IGfxCommandList* pCommandList)
        {
            RGTexture* radiance = pRenderGraph->GetTexture(data.radiance);
            RGTexture* normal = pRenderGraph->GetTexture(data.normal);
            RGTexture* linearDepth = pRenderGraph->GetTexture(data.linearDepth);
            RGTexture* velocity = pRenderGraph->GetTexture(data.velocity);

            NRDUserPool userPool = {};
            userPool[(size_t)nrd::ResourceType::IN_MV] = { velocity->GetTexture(), velocity->GetSRV(), velocity->GetUAV(), GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_NORMAL_ROUGHNESS] = { normal->GetTexture(), normal->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_VIEWZ] = { linearDepth->GetTexture(), linearDepth->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST] = { radiance->GetTexture(), radiance->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST] = { m_pOutputRadiance->GetTexture(), m_pOutputRadiance->GetSRV(), m_pOutputRadiance->GetUAV(), GfxResourceState::UnorderedAccess };

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            nrd::CommonSettings commonSettings = {};
            memcpy(commonSettings.viewToClipMatrix, &camera->GetNonJitterProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.viewToClipMatrixPrev, &camera->GetNonJitterPrevProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrix, &camera->GetViewMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrixPrev, &camera->GetPrevViewMatrix(), sizeof(float4x4));
            commonSettings.motionVectorScale[0] = -0.5f;
            commonSettings.motionVectorScale[1] = 0.5f;
            commonSettings.motionVectorScale[2] = 0.0; //todo
            commonSettings.cameraJitter[0] = camera->GetJitter().x;
            commonSettings.cameraJitter[1] = camera->GetJitter().y;
            commonSettings.frameIndex = (uint32_t)m_pRenderer->GetFrameID();
            commonSettings.accumulationMode = nrd::AccumulationMode::CONTINUE;
            commonSettings.isMotionVectorInWorldSpace = false;            

            m_pReblur->SetMethodSettings(NRDMethod, m_pReblurSettings.get());
            m_pReblur->Denoise(pCommandList, commonSettings, userPool);
            m_bHistoryInvalid = false;

            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    struct ResolveData
    {
        RGHandle denoiserOutput;
        RGHandle resolveOutput;
    };

    auto resolve_pass = pRenderGraph->AddPass<ResolveData>("ReBLUR - resolve", RenderPassType::Compute,
        [&](ResolveData& data, RGBuilder& builder)
        {
            data.denoiserOutput = builder.Read(reblur_pass->output);
            data.resolveOutput = builder.Write(m_historyIrradiance);
        },
        [=](const ResolveData& data, IGfxCommandList* pCommandList)
        {
            ResolveOutput(pCommandList, width, height);
        });

    return resolve_pass->resolveOutput;
}

void GIDenoiserNRD::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    m_bNeedCreateReblur = true;
}

void GIDenoiserNRD::CreateReblurDenoiser(uint32_t width, uint32_t height)
{
    if (m_bNeedCreateReblur)
    {
        m_pReblur->Destroy();

        const nrd::MethodDesc methodDescs[] =
        {
            { NRDMethod, (uint16_t)width, (uint16_t)height },
        };

        nrd::DenoiserCreationDesc desc = {};
        desc.requestedMethods = methodDescs;
        desc.requestedMethodsNum = 1;
        m_pReblur->Initialize(desc);

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

void GIDenoiserNRD::PackRadiance(IGfxCommandList* pCommandList, RGTexture* radiance, RGTexture* linearDepth, RGTexture* packedRadiance, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pPackRadianceHitTPSO);

    struct CB
    {
        float4 hitDistParams;
        uint radianceTexture;
        uint linearDepthTexture;
        uint packedRadianceTexture;
    };

    CB cb;
    memcpy(&cb.hitDistParams, &m_pReblurSettings->hitDistanceParameters, sizeof(float4));
    cb.radianceTexture = radiance->GetSRV()->GetHeapIndex();
    cb.linearDepthTexture = linearDepth->GetSRV()->GetHeapIndex();
    cb.packedRadianceTexture = packedRadiance->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}

void GIDenoiserNRD::ResolveOutput(IGfxCommandList* pCommandList, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_pResolveOutputPSO);

    uint32_t cb[2] = { m_pOutputRadiance->GetSRV()->GetHeapIndex(), m_pResolvedOutput->GetUAV()->GetHeapIndex() };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    pCommandList->Dispatch(DivideRoudingUp(width, 8), DivideRoudingUp(height, 8), 1);
}
