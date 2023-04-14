#include "gi_denoiser_nrd.h"
#include "nrd_integration.h"

GIDenoiserNRD::GIDenoiserNRD(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    m_pReblur = eastl::make_unique<NRDIntegration>(pRenderer);

    nrd::DenoiserCreationDesc desc;
    m_pReblur->Initialize(desc);
}

GIDenoiserNRD::~GIDenoiserNRD() = default;