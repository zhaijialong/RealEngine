#include "nrd_integration.h"
#include "../renderer.h"

NRDIntegration::NRDIntegration(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

bool NRDIntegration::Initialize(const nrd::DenoiserCreationDesc& denoiserCreationDesc)
{
    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();
    if (libraryDesc.versionMajor != NRD_VERSION_MAJOR || libraryDesc.versionMinor != NRD_VERSION_MINOR)
    {
        return false;
    }

    for (uint32_t i = 0; i < denoiserCreationDesc.requestedMethodsNum; i++)
    {
        uint32_t j = 0;
        for (; j < libraryDesc.supportedMethodsNum; j++)
        {
            if (libraryDesc.supportedMethods[j] == denoiserCreationDesc.requestedMethods[i].method)
            {
                break;
            }
        }

        if (j == libraryDesc.supportedMethodsNum)
        {
            return false;
        }
    }

    if (nrd::CreateDenoiser(denoiserCreationDesc, m_pDenoiser) != nrd::Result::SUCCESS)
    {
        return false;
    }

    CreatePipelines();
    CreateResources();

    return true;
}

void NRDIntegration::Destroy()
{
    if (m_pDenoiser)
    {
        nrd::DestroyDenoiser(*m_pDenoiser);
        m_pDenoiser = nullptr;
    }
}

bool NRDIntegration::SetMethodSettings(nrd::Method method, const void* methodSettings)
{
    nrd::Result result = nrd::SetMethodSettings(*m_pDenoiser, method, methodSettings);
    RE_ASSERT(result == nrd::Result::SUCCESS);

    return result == nrd::Result::SUCCESS;
}

void NRDIntegration::Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings)
{
}

void NRDIntegration::CreatePipelines()
{
    m_Shaders.clear();
    m_pipelines.clear();

    IGfxDevice* device = m_pRenderer->GetDevice();
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*m_pDenoiser);

    for (uint32_t i = 0; i < denoiserDesc.pipelinesNum; i++)
    {
        const nrd::PipelineDesc& nrdPipelineDesc = denoiserDesc.pipelines[i];
        const nrd::ComputeShaderDesc& nrdComputeShader = nrdPipelineDesc.computeShaderDXIL; //todo : supports spirv when we have a vulkan backend

        GfxShaderDesc shaderDesc;
        shaderDesc.file = nrdPipelineDesc.shaderFileName;
        shaderDesc.entry_point = nrdPipelineDesc.shaderEntryPointName;
        shaderDesc.profile = "cs_6_6";
        IGfxShader* shader = device->CreateShader(shaderDesc, { (uint8_t*)nrdComputeShader.bytecode, (size_t)nrdComputeShader.size }, "");
        m_Shaders.emplace_back(shader);

        GfxComputePipelineDesc pipelineDesc;
        pipelineDesc.cs = shader;
        IGfxPipelineState* pipeline = device->CreateComputePipelineState(pipelineDesc, nrdPipelineDesc.shaderFileName);
        m_pipelines.emplace_back(pipeline);
    }
}

void NRDIntegration::CreateResources()
{
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*m_pDenoiser);
    const uint32_t poolSize = denoiserDesc.permanentPoolSize + denoiserDesc.transientPoolSize;
}
