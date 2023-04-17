#include "nrd_integration.h"
#include "../renderer.h"
#include "../../utils/fmt.h"

//must match with NRD.hlsli : cbuffer globalHeapIndices : register(b1)
struct ResourceIndices
{
    uint4 gInputTextureIndices[7];
    uint4 gOutputTextureIndices[4];
    uint4 gSamplerIndices;

    void SetInputTexture(uint32_t bindingIndex, IGfxDescriptor* srv)
    {
        gInputTextureIndices[bindingIndex / 7][bindingIndex % 4] = srv->GetHeapIndex();
    }

    void SetOutputTexture(uint32_t bindingIndex, IGfxDescriptor* uav)
    {
        gOutputTextureIndices[bindingIndex / 4][bindingIndex % 4] = uav->GetHeapIndex();
    }

    void SetSampler(uint32_t bindingIndex, IGfxDescriptor* sampler)
    {
        gSamplerIndices[bindingIndex % 4] = sampler->GetHeapIndex();
    }
};

static inline GfxFormat GetFormat(nrd::Format format)
{
    switch (format)
    {
    case nrd::Format::R8_UNORM:
        return GfxFormat::R8UNORM;
    case nrd::Format::R8_SNORM:
        return GfxFormat::R8SNORM;
    case nrd::Format::R8_UINT:
        return GfxFormat::R8UI;
    case nrd::Format::R8_SINT:
        return GfxFormat::R8SI;
    case nrd::Format::RG8_UNORM:
        return GfxFormat::RG8UNORM;
    case nrd::Format::RG8_SNORM:
        return GfxFormat::RG8SNORM;
    case nrd::Format::RG8_UINT:
        return GfxFormat::RG8UI;
    case nrd::Format::RG8_SINT:
        return GfxFormat::RG8SI;
    case nrd::Format::RGBA8_UNORM:
        return GfxFormat::RGBA8UNORM;
    case nrd::Format::RGBA8_SNORM:
        return GfxFormat::RGBA8SNORM;
    case nrd::Format::RGBA8_UINT:
        return GfxFormat::RGBA8UI;
    case nrd::Format::RGBA8_SINT:
        return GfxFormat::RGBA8SI;
    case nrd::Format::RGBA8_SRGB:
        return GfxFormat::RGBA8SRGB;
    case nrd::Format::R16_UNORM:
        return GfxFormat::R16UNORM;
    case nrd::Format::R16_SNORM:
        return GfxFormat::R16SNORM;
    case nrd::Format::R16_UINT:
        return GfxFormat::R16UI;
    case nrd::Format::R16_SINT:
        return GfxFormat::R16SI;
    case nrd::Format::R16_SFLOAT:
        return GfxFormat::R16F;
    case nrd::Format::RG16_UNORM:
        return GfxFormat::RG16UNORM;
    case nrd::Format::RG16_SNORM:
        return GfxFormat::RG16SNORM;
    case nrd::Format::RG16_UINT:
        return GfxFormat::RG16UI;
    case nrd::Format::RG16_SINT:
        return GfxFormat::RG16SI;
    case nrd::Format::RG16_SFLOAT:
        return GfxFormat::RG16F;
    case nrd::Format::RGBA16_UNORM:
        return GfxFormat::RGBA16UNORM;
    case nrd::Format::RGBA16_SNORM:
        return GfxFormat::RGBA16SNORM;
    case nrd::Format::RGBA16_UINT:
        return GfxFormat::RGBA16UI;
    case nrd::Format::RGBA16_SINT:
        return GfxFormat::RGBA16SI;
    case nrd::Format::RGBA16_SFLOAT:
        return GfxFormat::RGBA16F;
    case nrd::Format::R32_UINT:
        return GfxFormat::R32UI;
    case nrd::Format::R32_SINT:
        return GfxFormat::R32SI;
    case nrd::Format::R32_SFLOAT:
        return GfxFormat::R32F;
    case nrd::Format::RG32_UINT:
        return GfxFormat::RG32UI;
    case nrd::Format::RG32_SINT:
        return GfxFormat::RG32SI;
    case nrd::Format::RG32_SFLOAT:
        return GfxFormat::RG32F;
    case nrd::Format::RGB32_UINT:
        return GfxFormat::RGB32UI;
    case nrd::Format::RGB32_SINT:
        return GfxFormat::RGB32SI;
    case nrd::Format::RGB32_SFLOAT:
        return GfxFormat::RGB32F;
    case nrd::Format::RGBA32_UINT:
        return GfxFormat::RGBA32UI;
    case nrd::Format::RGBA32_SINT:
        return GfxFormat::RGBA32SI;
    case nrd::Format::RGBA32_SFLOAT:
        return GfxFormat::RGBA32F;
    case nrd::Format::R10_G10_B10_A2_UNORM:
        return GfxFormat::RGB10A2UNORM;
    case nrd::Format::R10_G10_B10_A2_UINT:
        return GfxFormat::RGB10A2UI;
    case nrd::Format::R11_G11_B10_UFLOAT:
        return GfxFormat::R11G11B10F;
    case nrd::Format::R9_G9_B9_E5_UFLOAT:
        return GfxFormat::RGB9E5;
    case nrd::Format::MAX_NUM:
    default:
        RE_ASSERT(false);
        return GfxFormat::Unknown;
    }
}

NRDIntegration::NRDIntegration(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

NRDIntegration::~NRDIntegration()
{
    Destroy();
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
    CreateTextures();
    CreateSamplers();

    return true;
}

void NRDIntegration::Destroy()
{
    m_shaders.clear();
    m_pipelines.clear();
    m_samplers.clear();
    m_textures.clear();

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

void NRDIntegration::Denoise(IGfxCommandList* pCommandList, const nrd::CommonSettings& commonSettings, NRDUserPool& userPool)
{
    const nrd::DispatchDesc* dispatchDescs = nullptr;
    uint32_t dispatchDescNum = 0;
    nrd::GetComputeDispatches(*m_pDenoiser, commonSettings, dispatchDescs, dispatchDescNum);

    for (uint32_t i = 0; i < dispatchDescNum; i++)
    {
        const nrd::DispatchDesc& dispatchDesc = dispatchDescs[i];

        Dispatch(pCommandList, dispatchDesc, userPool);
    }
}

void NRDIntegration::CreatePipelines()
{
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
        m_shaders.emplace_back(shader);

        GfxComputePipelineDesc pipelineDesc;
        pipelineDesc.cs = shader;
        IGfxPipelineState* pipeline = device->CreateComputePipelineState(pipelineDesc, nrdPipelineDesc.shaderFileName);
        m_pipelines.emplace_back(pipeline);
    }
}

void NRDIntegration::CreateTextures()
{
    IGfxDevice* device = m_pRenderer->GetDevice();
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*m_pDenoiser);

    const uint32_t poolSize = denoiserDesc.permanentPoolSize + denoiserDesc.transientPoolSize;
    for (uint32_t i = 0; i < poolSize; i++)
    {
        const nrd::TextureDesc& nrdTextureDesc = (i < denoiserDesc.permanentPoolSize) ? denoiserDesc.permanentPool[i] : denoiserDesc.transientPool[i - denoiserDesc.permanentPoolSize];

        GfxFormat format = GetFormat(nrdTextureDesc.format);

        eastl::string name;
        if (i < denoiserDesc.permanentPoolSize)
        {
            name = fmt::format("NRD::PermamentPool{}", i).c_str();
        }
        else
        {
            name = fmt::format("NRD::TransientPool{}", i - denoiserDesc.permanentPoolSize).c_str();
        }

        NRDPoolTexture texture;
        texture.texture.reset(m_pRenderer->CreateTexture2D(nrdTextureDesc.width, nrdTextureDesc.height, nrdTextureDesc.mipNum, format, GfxTextureUsageUnorderedAccess, name));
        for (uint16_t i = 0; i < nrdTextureDesc.mipNum; ++i)
        {
            texture.states.push_back(GfxResourceState::UnorderedAccess);
        }

        m_textures.push_back(eastl::move(texture));
    }
}

void NRDIntegration::CreateSamplers()
{
    IGfxDevice* device = m_pRenderer->GetDevice();
    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*m_pDenoiser);

    for (uint32_t i = 0; i < denoiserDesc.samplersNum; i++)
    {
        nrd::Sampler nrdSampler = denoiserDesc.samplers[i];

        GfxSamplerDesc desc;

        if (nrdSampler == nrd::Sampler::NEAREST_CLAMP || nrdSampler == nrd::Sampler::LINEAR_CLAMP)
        {
            desc.address_u = GfxSamplerAddressMode::ClampToEdge;
            desc.address_v = GfxSamplerAddressMode::ClampToEdge;
            desc.address_w = GfxSamplerAddressMode::ClampToEdge;
        }
        else
        {
            desc.address_u = GfxSamplerAddressMode::MirroredRepeat;
            desc.address_v = GfxSamplerAddressMode::MirroredRepeat;
            desc.address_w = GfxSamplerAddressMode::MirroredRepeat;
        }

        if (nrdSampler == nrd::Sampler::NEAREST_CLAMP || nrdSampler == nrd::Sampler::NEAREST_MIRRORED_REPEAT)
        {
            desc.min_filter = GfxFilter::Point;
            desc.mag_filter = GfxFilter::Point;
            desc.mip_filter = GfxFilter::Point;
        }
        else
        {
            desc.min_filter = GfxFilter::Linear;
            desc.mag_filter = GfxFilter::Linear;
            desc.mip_filter = GfxFilter::Linear;
        }

        IGfxDescriptor* sampler = device->CreateSampler(desc, "NRD sampler");
        m_samplers.emplace_back(sampler);
    }
}

void NRDIntegration::Dispatch(IGfxCommandList* pCommandList, const nrd::DispatchDesc& dispatchDesc, NRDUserPool& userPool)
{
    GPU_EVENT(pCommandList, dispatchDesc.name);

    const nrd::DenoiserDesc& denoiserDesc = nrd::GetDenoiserDesc(*m_pDenoiser);
    const nrd::PipelineDesc& pipelineDesc = denoiserDesc.pipelines[dispatchDesc.pipelineIndex];

    IGfxPipelineState* pso = m_pipelines[dispatchDesc.pipelineIndex].get();
    pCommandList->SetPipelineState(pso);

    ResourceIndices resources = {};

    uint32_t n = 0;
    for (uint32_t i = 0; i < pipelineDesc.resourceRangesNum; i++)
    {
        const nrd::ResourceRangeDesc& resourceRange = pipelineDesc.resourceRanges[i];
        const bool isStorage = resourceRange.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE;

        for (uint32_t j = 0; j < resourceRange.descriptorsNum; j++)
        {
            const nrd::ResourceDesc& nrdResource = dispatchDesc.resources[n++];
            GfxResourceState stateNeeded = nrdResource.stateNeeded == nrd::DescriptorType::STORAGE_TEXTURE ? GfxResourceState::UnorderedAccess : GfxResourceState::ShaderResourceNonPS;

            IGfxDescriptor* descriptor = nullptr;

            if (nrdResource.type == nrd::ResourceType::TRANSIENT_POOL || nrdResource.type == nrd::ResourceType::PERMANENT_POOL)
            {
                NRDPoolTexture* nrdTexture = nullptr;
                if (nrdResource.type == nrd::ResourceType::TRANSIENT_POOL)
                {
                    nrdTexture = &m_textures[nrdResource.indexInPool + denoiserDesc.permanentPoolSize];
                }
                else if (nrdResource.type == nrd::ResourceType::PERMANENT_POOL)
                {
                    nrdTexture = &m_textures[nrdResource.indexInPool];
                }

                nrdTexture->SetResourceSate(pCommandList, nrdResource.mipOffset, stateNeeded);

                if (isStorage)
                {
                    descriptor = nrdTexture->texture->GetUAV(nrdResource.mipOffset);
                }
                else
                {
                    RE_ASSERT(nrdResource.mipOffset == 0); //Texture2D doesn't have SRVs for a specific mip level now
                    descriptor = nrdTexture->texture->GetSRV();
                }
            }
            else
            {
                NRDUserTexture* userTexture = &userPool[(size_t)nrdResource.type];
                userTexture->SetResourceSate(pCommandList, stateNeeded);

                descriptor = isStorage ? userTexture->uav : userTexture->srv;
            }

            if (isStorage)
            {
                resources.SetOutputTexture(resourceRange.baseRegisterIndex + j, descriptor);
            }
            else
            {
                resources.SetInputTexture(resourceRange.baseRegisterIndex + j, descriptor);
            }
        }
    }

    for (size_t i = 0; i < m_samplers.size(); ++i)
    {
        resources.SetSampler(i, m_samplers[i].get());
    }

    pCommandList->SetComputeConstants(1, &resources, sizeof(resources));

    if (dispatchDesc.constantBufferDataSize > 0)
    {
        pCommandList->SetComputeConstants(2, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);
    }

    pCommandList->Dispatch(dispatchDesc.gridWidth, dispatchDesc.gridHeight, 1);
}

