#include "clear_uav.h"
#include "renderer.h"
#include "core/engine.h"

static inline eastl::string GetEntryPoint(GfxUnorderedAccessViewType uavType)
{
    switch (uavType)
    {
    case GfxUnorderedAccessViewType::Texture2D:
        return "clear_texture2d";
    case GfxUnorderedAccessViewType::Texture2DArray:
        return "clear_texture2d_array";
    case GfxUnorderedAccessViewType::Texture3D:
        return "clear_texture3d";
    case GfxUnorderedAccessViewType::TypedBuffer:
        return "clear_typed_buffer";
    case GfxUnorderedAccessViewType::RawBuffer:
        return "clear_raw_buffer";
    default:
        RE_ASSERT(false);
        return "";
    }
}

static inline eastl::string GetTypeDefine(GfxFormat format)
{
    switch (format)
    {
    case GfxFormat::R32F:
    case GfxFormat::R16F:
    case GfxFormat::R16UNORM:
    case GfxFormat::R8UNORM:
        return "UAV_TYPE_FLOAT";
    case GfxFormat::RG32F:
    case GfxFormat::RG16F:
    case GfxFormat::RG16UNORM:
    case GfxFormat::RG8UNORM:
        return "UAV_TYPE_FLOAT2";
    case GfxFormat::R11G11B10F:
        return "UAV_TYPE_FLOAT3";
    case GfxFormat::RGBA32F:
    case GfxFormat::RGBA16F:
    case GfxFormat::RGBA16UNORM:
    case GfxFormat::RGBA8UNORM:
        return "UAV_TYPE_FLOAT4";
    case GfxFormat::R32UI:
    case GfxFormat::R16UI:
    case GfxFormat::R8UI:
        return "UAV_TYPE_UINT";
    case GfxFormat::RG32UI:
    case GfxFormat::RG16UI:
    case GfxFormat::RG8UI:
        return "UAV_TYPE_UINT2";
    case GfxFormat::RGB32UI:
        return "UAV_TYPE_UINT3";
    case GfxFormat::RGBA32UI:
    case GfxFormat::RGBA16UI:
    case GfxFormat::RGBA8UI:
        return "UAV_TYPE_UINT4";
    default:
        RE_ASSERT(false);
        return "";
    }
}

template<typename T>
static inline IGfxShader* GetShader(const GfxUnorderedAccessViewDesc& uavDesc)
{
    eastl::string entryPoint = GetEntryPoint(uavDesc.type);
    eastl::vector<eastl::string> defines;
    
    if (uavDesc.type != GfxUnorderedAccessViewType::RawBuffer)
    {
        defines.push_back(GetTypeDefine(uavDesc.format));
    }
    else
    {
        defines.push_back(eastl::is_same<T, float>::value ? "UAV_TYPE_FLOAT4" : "UAV_TYPE_UINT4");
    }

    Renderer* renderer = Engine::GetInstance()->GetRenderer();
    return renderer->GetShader("clear_uav.hlsl", entryPoint, GfxShaderType::CS, defines);
}

static uint3 GetDispatchGroupCount(IGfxResource* resource, const GfxUnorderedAccessViewDesc& uavDesc)
{
    switch (uavDesc.type)
    {
    case GfxUnorderedAccessViewType::Texture2D:
    {
        const GfxTextureDesc& textureDesc = ((IGfxTexture*)resource)->GetDesc();
        return uint3(DivideRoudingUp(textureDesc.width, 8), DivideRoudingUp(textureDesc.height, 8), 1);
    }
    case GfxUnorderedAccessViewType::Texture2DArray:
    {
        const GfxTextureDesc& textureDesc = ((IGfxTexture*)resource)->GetDesc();
        return uint3(DivideRoudingUp(textureDesc.width, 8), DivideRoudingUp(textureDesc.height, 8), uavDesc.texture.array_size);
    }
    case GfxUnorderedAccessViewType::Texture3D:
    {
        const GfxTextureDesc& textureDesc = ((IGfxTexture*)resource)->GetDesc();
        return uint3(DivideRoudingUp(textureDesc.width, 8), DivideRoudingUp(textureDesc.height, 8), DivideRoudingUp(textureDesc.depth, 8));
    }
    case GfxUnorderedAccessViewType::TypedBuffer:
    {
        const GfxBufferDesc& bufferDesc = ((IGfxBuffer*)resource)->GetDesc();
        uint32_t elementCount = uavDesc.buffer.size / bufferDesc.stride;
        return uint3(DivideRoudingUp(elementCount, 64), 1, 1);
    }
    case GfxUnorderedAccessViewType::RawBuffer:
    {
        uint32_t elementCount = uavDesc.buffer.size / 16; // uint4 or float4
        return uint3(DivideRoudingUp(elementCount, 64), 1, 1);
    }
    default:
        RE_ASSERT(false);
        return uint3();
    }
}

template<typename T>
void ClearUAVImpl(IGfxCommandList* commandList, IGfxResource* resource, IGfxDescriptor* descriptor, const GfxUnorderedAccessViewDesc& uavDesc, const T* value)
{
    GPU_EVENT(commandList, "ClearUAV");

    Renderer* renderer = Engine::GetInstance()->GetRenderer();

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = GetShader<T>(uavDesc);
    commandList->SetPipelineState(renderer->GetPipelineState(psoDesc, "Clear UAV"));

    uint32_t uavIndex = descriptor->GetHeapIndex();
    commandList->SetComputeConstants(0, &uavIndex, sizeof(uint32_t));
    commandList->SetComputeConstants(1, value, sizeof(T) * 4);

    uint3 groupCount = GetDispatchGroupCount(resource, uavDesc);
    commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);
}

void ClearUAV(IGfxCommandList* commandList, IGfxResource* resource, IGfxDescriptor* descriptor, const GfxUnorderedAccessViewDesc& uavDesc, const float* value)
{
    ClearUAVImpl<float>(commandList, resource, descriptor, uavDesc, value);
}

void ClearUAV(IGfxCommandList* commandList, IGfxResource* resource, IGfxDescriptor* descriptor, const GfxUnorderedAccessViewDesc& uavDesc, const uint32_t* value)
{
    ClearUAVImpl<uint32_t>(commandList, resource, descriptor, uavDesc, value);
}
