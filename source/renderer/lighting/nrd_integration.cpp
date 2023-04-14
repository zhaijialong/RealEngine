#include "nrd_integration.h"

NRDIntegration::NRDIntegration(Renderer* pRenderer)
{
}

bool NRDIntegration::Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc)
{
    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();
    if (libraryDesc.versionMajor != NRD_VERSION_MAJOR || libraryDesc.versionMinor != NRD_VERSION_MINOR)
    {
        return false;
    }

    return true;
}

bool NRDIntegration::SetMethodSettings(nrd::Method method, const void* methodSettings)
{
    return false;
}

void NRDIntegration::Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings)
{
}
