#include "gi_denoiser_nrd.h"
#include "nrd_integration.h"
#include "utils/gui_util.h"
#include "../renderer.h"

static const nrd::Method NRDMethod = nrd::Method::REBLUR_DIFFUSE; //todo : REBLUR_DIFFUSE_SH

GIDenoiserNRD::GIDenoiserNRD(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pReblur = eastl::make_unique<NRDIntegration>(pRenderer);

    Engine::GetInstance()->WindowResizeSignal.connect(&GIDenoiserNRD::OnWindowResize, this);

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("nrd_integration.hlsl", "pack_normal_roughness", "cs_6_6", {});
    m_pPackNormalRoughnessPSO = pRenderer->GetPipelineState(psoDesc, "ReBlur - pack normal/roughness PSO");
}

GIDenoiserNRD::~GIDenoiserNRD() = default;

RGHandle GIDenoiserNRD::Render(RenderGraph* pRenderGraph, RGHandle radiance, RGHandle normal, RGHandle linearDepth, RGHandle velocity, uint32_t width, uint32_t height)
{
    CreateReblurDenoiser(width, height);

    struct ReblurData
    {
        RGHandle radiance;
        RGHandle normal;
        RGHandle linearDepth;
        RGHandle velocity;
        RGHandle output;
    };

    auto reblur_pass = pRenderGraph->AddPass<ReblurData>("ReBLUR Denoiser", RenderPassType::Compute,
        [&](ReblurData& data, RGBuilder& builder)
        {
            //todo : pack in NRD's format
            data.radiance = builder.Read(radiance);
            data.normal = builder.Read(normal);
            data.linearDepth = builder.Read(linearDepth);
            data.velocity = builder.Read(velocity);

            RGTexture::Desc desc;
            desc.width = width;
            desc.height = height;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Write(builder.Create<RGTexture>(desc, "ReBlur - output"));
        },
        [=](const ReblurData& data, IGfxCommandList* pCommandList)
        {
            RGTexture* radiance = pRenderGraph->GetTexture(data.radiance);
            RGTexture* normal = pRenderGraph->GetTexture(data.normal);
            RGTexture* linearDepth = pRenderGraph->GetTexture(data.linearDepth);
            RGTexture* velocity = pRenderGraph->GetTexture(data.velocity);
            RGTexture* output = pRenderGraph->GetTexture(data.output);

            NRDUserPool userPool;
            userPool[(size_t)nrd::ResourceType::IN_MV] = { velocity->GetTexture(), velocity->GetSRV(), velocity->GetUAV(), GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_NORMAL_ROUGHNESS] = { normal->GetTexture(), normal->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_VIEWZ] = { linearDepth->GetTexture(), linearDepth->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST] = { radiance->GetTexture(), radiance->GetSRV(), nullptr, GfxResourceState::ShaderResourceNonPS };
            userPool[(size_t)nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST] = { output->GetTexture(), output->GetSRV(), output->GetUAV(), GfxResourceState::ShaderResourceNonPS };

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            nrd::CommonSettings commonSettings = {};
            memcpy(commonSettings.viewToClipMatrix, &camera->GetNonJitterViewProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.viewToClipMatrixPrev, &camera->GetNonJitterPrevViewProjectionMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrix, &camera->GetViewMatrix(), sizeof(float4x4));
            memcpy(commonSettings.worldToViewMatrixPrev, &camera->GetPrevViewMatrix(), sizeof(float4x4));
            commonSettings.motionVectorScale[0] = -0.5f / width;
            commonSettings.motionVectorScale[1] = 0.5f / height;
            commonSettings.motionVectorScale[2] = 0.0; //todo
            commonSettings.cameraJitter[0] = camera->GetJitter().x;
            commonSettings.cameraJitter[1] = camera->GetJitter().y;
            commonSettings.frameIndex = (uint32_t)m_pRenderer->GetFrameID();
            commonSettings.accumulationMode = nrd::AccumulationMode::CONTINUE;
            commonSettings.isMotionVectorInWorldSpace = false;

            nrd::ReblurSettings reblurSettings = {};

            m_pReblur->SetMethodSettings(NRDMethod, &reblurSettings);
            m_pReblur->Denoise(pCommandList, commonSettings, userPool);
        });

    return reblur_pass->output;
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
