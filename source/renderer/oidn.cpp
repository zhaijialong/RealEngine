#include "oidn.h"
#if WITH_OIDN

#include "renderer.h"
#include "utils/log.h"
#include "magic_enum/magic_enum.hpp"
#include "sokol/sokol_time.h"

static void OnErrorCallback(void* userPtr, OIDNError code, const char* message)
{
    RE_ERROR("{} : {}", magic_enum::enum_name(code), message);
}

OIDN::OIDN(Renderer* renderer)
{
    m_renderer = renderer;
    m_fence.reset(renderer->GetDevice()->CreateFence("OIDN.fence"));

    m_device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    //m_device = oidnNewDevice(OIDN_DEVICE_TYPE_CPU);
    if (m_device)
    {
        oidnSetDeviceErrorFunction(m_device, OnErrorCallback, nullptr);
        oidnCommitDevice(m_device);

        m_filter = oidnNewFilter(m_device, "RT");
    }
}

OIDN::~OIDN()
{
    ReleaseBuffers();
    oidnReleaseFilter(m_filter);
    oidnReleaseDevice(m_device);
}

void OIDN::Execute(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    InitBuffers(color, albedo, normal);

    bool cpuDevice = oidnGetDeviceInt(m_device, "type") == OIDN_DEVICE_TYPE_CPU;
    if (cpuDevice)
    {
        ExecuteCPU(commandList, color, albedo, normal);
    }
    else
    {
        ExecuteGPU(commandList, color, albedo, normal);
    }
}

void OIDN::ExecuteCPU(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    if (!m_bDenoised)
    {
        m_bDenoised = true;

        commandList->TextureBarrier(color, 0, GfxAccessCopyDst, GfxAccessCopySrc);
        commandList->CopyTextureToBuffer(m_colorBuffer.get(), 0, color, 0, 0);
        commandList->CopyTextureToBuffer(m_albedoBuffer.get(), 0, albedo, 0, 0);
        commandList->CopyTextureToBuffer(m_normalBuffer.get(), 0, normal, 0, 0);
        commandList->End();
        commandList->Signal(m_fence.get(), ++m_fenceValue);
        commandList->Submit();
        m_fence->Wait(m_fenceValue); // to ensure the copy has finished

        void* colorData = oidnGetBufferData(m_oidnColorBuffer);
        
        memcpy(colorData, m_colorBuffer->GetCpuAddress(), oidnGetBufferSize(m_oidnColorBuffer));
        memcpy(oidnGetBufferData(m_oidnAlbedoBuffer), m_albedoBuffer->GetCpuAddress(), oidnGetBufferSize(m_oidnAlbedoBuffer));
        memcpy(oidnGetBufferData(m_oidnNormalBuffer), m_normalBuffer->GetCpuAddress(), oidnGetBufferSize(m_oidnNormalBuffer));

        uint64_t ticks = stm_now();
        oidnExecuteFilter(m_filter);
        RE_INFO("OIDN CPU : {} ms", stm_ms(stm_now() - ticks));

        memcpy(m_colorUploadBuffer->GetCpuAddress(), colorData, oidnGetBufferSize(m_oidnColorBuffer));

        commandList->Begin();
        commandList->TextureBarrier(color, 0, GfxAccessCopySrc, GfxAccessCopyDst);
        m_renderer->SetupGlobalConstants(commandList);
    }

    commandList->CopyBufferToTexture(color, 0, 0, m_colorUploadBuffer.get(), 0);
}

void OIDN::ExecuteGPU(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    if (!m_bDenoised)
    {
        m_bDenoised = true;

        commandList->TextureBarrier(color, 0, GfxAccessCopyDst, GfxAccessCopySrc);
        commandList->CopyTextureToBuffer(m_colorBuffer.get(), 0, color, 0, 0);
        commandList->CopyTextureToBuffer(m_albedoBuffer.get(), 0, albedo, 0, 0);
        commandList->CopyTextureToBuffer(m_normalBuffer.get(), 0, normal, 0, 0);
        commandList->End();
        commandList->Signal(m_fence.get(), ++m_fenceValue);
        commandList->Submit();
        m_fence->Wait(m_fenceValue); // to ensure the copy has finished

        uint64_t ticks = stm_now();
        oidnExecuteFilter(m_filter);
        RE_INFO("OIDN GPU : {} ms", stm_ms(stm_now() - ticks));

        commandList->Begin();
        commandList->TextureBarrier(color, 0, GfxAccessCopySrc, GfxAccessCopyDst);
        m_renderer->SetupGlobalConstants(commandList);
    }

    commandList->CopyBufferToTexture(color, 0, 0, m_colorBuffer.get(), 0);
}

void OIDN::InitBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    if (m_oidnColorBuffer == nullptr ||
        oidnGetBufferSize(m_oidnColorBuffer) != color->GetRequiredStagingBufferSize())
    {
        ReleaseBuffers();

        if (oidnGetDeviceInt(m_device, "type") == OIDN_DEVICE_TYPE_CPU)
        {
            InitCPUBuffers(color, albedo, normal);
        }
        else
        {
            InitGPUBuffers(color, albedo, normal);
        }

        const uint32_t width = color->GetDesc().width;
        const uint32_t height = color->GetDesc().height;

        oidnSetFilterImage(m_filter, "color", m_oidnColorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, color->GetRowPitch());
        oidnSetFilterImage(m_filter, "albedo", m_oidnAlbedoBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, albedo->GetRowPitch());
        oidnSetFilterImage(m_filter, "normal", m_oidnNormalBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, normal->GetRowPitch());
        oidnSetFilterImage(m_filter, "output", m_oidnColorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, color->GetRowPitch());

        oidnSetFilterBool(m_filter, "hdr", true);
        oidnSetFilterBool(m_filter, "cleanAux", true);
        oidnCommitFilter(m_filter);
    }
}

void OIDN::ReleaseBuffers()
{
    if (m_oidnColorBuffer)
    {
        oidnReleaseBuffer(m_oidnColorBuffer);
    }

    if (m_oidnAlbedoBuffer)
    {
        oidnReleaseBuffer(m_oidnAlbedoBuffer);
    }

    if (m_oidnNormalBuffer)
    {
        oidnReleaseBuffer(m_oidnNormalBuffer);
    }
}

void OIDN::InitCPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    IGfxDevice* device = m_renderer->GetDevice();

    GfxBufferDesc desc;
    desc.size = color->GetRequiredStagingBufferSize();
    desc.memory_type = GfxMemoryType::GpuToCpu;

    m_colorBuffer.reset(device->CreateBuffer(desc, "OIDN.colorBuffer"));
    m_albedoBuffer.reset(device->CreateBuffer(desc, "OIDN.albedoBuffer"));
    m_normalBuffer.reset(device->CreateBuffer(desc, "OIDN.normalBuffer"));

    m_oidnColorBuffer = oidnNewBuffer(m_device, desc.size);
    m_oidnAlbedoBuffer = oidnNewBuffer(m_device, desc.size);
    m_oidnNormalBuffer = oidnNewBuffer(m_device, desc.size);

    desc.memory_type = GfxMemoryType::CpuOnly;
    m_colorUploadBuffer.reset(device->CreateBuffer(desc, "OIDN.colorUploadBuffer"));
}

void OIDN::InitGPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    IGfxDevice* device = m_renderer->GetDevice();

    GfxBufferDesc desc;
    desc.size = color->GetRequiredStagingBufferSize();
    desc.memory_type = GfxMemoryType::GpuOnly;
    desc.alloc_type = GfxAllocationType::Committed;
    desc.usage = GfxBufferUsageShared;

    m_colorBuffer.reset(device->CreateBuffer(desc, "OIDN.colorBuffer"));
    m_albedoBuffer.reset(device->CreateBuffer(desc, "OIDN.albedoBuffer"));
    m_normalBuffer.reset(device->CreateBuffer(desc, "OIDN.normalBuffer"));

    m_oidnColorBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_colorBuffer->GetSharedHandle(), nullptr, desc.size);
    m_oidnAlbedoBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_albedoBuffer->GetSharedHandle(), nullptr, desc.size);
    m_oidnNormalBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_normalBuffer->GetSharedHandle(), nullptr, desc.size);
}

#endif // #if WITH_OIDN