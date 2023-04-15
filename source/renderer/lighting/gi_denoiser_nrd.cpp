#include "gi_denoiser_nrd.h"
#include "nrd_integration.h"
#include "utils/gui_util.h"
#include "../renderer.h"

GIDenoiserNRD::GIDenoiserNRD(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pReblur = eastl::make_unique<NRDIntegration>(pRenderer);

    Engine::GetInstance()->WindowResizeSignal.connect(&GIDenoiserNRD::OnWindowResize, this);
}

GIDenoiserNRD::~GIDenoiserNRD() = default;

RGHandle GIDenoiserNRD::Render(RenderGraph* pRenderGraph, RGHandle radiance, uint32_t width, uint32_t height)
{
    CreateReblurDenoiser(width, height);


    return radiance;
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
            //{ nrd::Method::REBLUR_DIFFUSE_SH, (uint16_t)width, (uint16_t)height },
            { nrd::Method::REBLUR_DIFFUSE, (uint16_t)width, (uint16_t)height },
        };

        nrd::DenoiserCreationDesc desc = {};
        desc.requestedMethods = methodDescs;
        desc.requestedMethodsNum = 1;
        m_pReblur->Initialize(desc);

        m_bNeedCreateReblur = false;
    }
}
