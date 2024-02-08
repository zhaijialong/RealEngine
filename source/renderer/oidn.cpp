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
        commandList->CopyTextureToBuffer(m_colorReadbackBuffer.get(), 0, color, 0, 0);
        commandList->CopyTextureToBuffer(m_albedoReadbackBuffer.get(), 0, albedo, 0, 0);
        commandList->CopyTextureToBuffer(m_normalReadbackBuffer.get(), 0, normal, 0, 0);
        commandList->End();
        commandList->Signal(m_fence.get(), ++m_fenceValue);
        commandList->Submit();
        m_fence->Wait(m_fenceValue); // to ensure the copy has finished

        void* colorData = oidnGetBufferData(m_colorBuffer);
        
        memcpy(colorData, m_colorReadbackBuffer->GetCpuAddress(), oidnGetBufferSize(m_colorBuffer));
        memcpy(oidnGetBufferData(m_albedoBuffer), m_albedoReadbackBuffer->GetCpuAddress(), oidnGetBufferSize(m_albedoBuffer));
        memcpy(oidnGetBufferData(m_normalBuffer), m_normalReadbackBuffer->GetCpuAddress(), oidnGetBufferSize(m_normalBuffer));

        uint64_t ticks = stm_now();
        oidnExecuteFilter(m_filter);
        RE_INFO("OIDN CPU : {} ms", stm_ms(stm_now() - ticks));

        memcpy(m_colorUploadBuffer->GetCpuAddress(), colorData, oidnGetBufferSize(m_colorBuffer));

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
        commandList->CopyTexture(m_colorTexture.get(), 0, 0, color, 0, 0);
        commandList->CopyTexture(m_albedoTexture.get(), 0, 0, albedo, 0, 0);
        commandList->CopyTexture(m_normalTexture.get(), 0, 0, normal, 0, 0);
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

    commandList->CopyTexture(color, 0, 0, m_colorTexture.get(), 0, 0);
}

void OIDN::InitBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    if (m_colorBuffer == nullptr ||
        oidnGetBufferSize(m_colorBuffer) != color->GetRequiredStagingBufferSize())
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

        oidnSetFilterImage(m_filter, "color", m_colorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, color->GetRowPitch(0));
        oidnSetFilterImage(m_filter, "albedo", m_albedoBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, albedo->GetRowPitch(0));
        oidnSetFilterImage(m_filter, "normal", m_normalBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, normal->GetRowPitch(0));
        oidnSetFilterImage(m_filter, "output", m_colorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, color->GetRowPitch(0));

        oidnSetFilterBool(m_filter, "hdr", true);
        oidnSetFilterBool(m_filter, "cleanAux", true);
        oidnCommitFilter(m_filter);
    }
}

void OIDN::ReleaseBuffers()
{
    if (m_colorBuffer)
    {
        oidnReleaseBuffer(m_colorBuffer);
    }

    if (m_albedoBuffer)
    {
        oidnReleaseBuffer(m_albedoBuffer);
    }

    if (m_normalBuffer)
    {
        oidnReleaseBuffer(m_normalBuffer);
    }
}

void OIDN::InitCPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    IGfxDevice* device = m_renderer->GetDevice();

    GfxBufferDesc desc;
    desc.size = color->GetRequiredStagingBufferSize();
    desc.memory_type = GfxMemoryType::GpuToCpu;

    m_colorReadbackBuffer.reset(device->CreateBuffer(desc, "OIDN.colorReadbackBuffer"));
    m_albedoReadbackBuffer.reset(device->CreateBuffer(desc, "OIDN.albedoReadbackBuffer"));
    m_normalReadbackBuffer.reset(device->CreateBuffer(desc, "OIDN.normalReadbackBuffer"));

    m_colorBuffer = oidnNewBuffer(m_device, desc.size);
    m_albedoBuffer = oidnNewBuffer(m_device, desc.size);
    m_normalBuffer = oidnNewBuffer(m_device, desc.size);

    desc.memory_type = GfxMemoryType::CpuOnly;
    m_colorUploadBuffer.reset(device->CreateBuffer(desc, "OIDN.colorUploadBuffer"));
}

void OIDN::InitGPUBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    IGfxDevice* device = m_renderer->GetDevice();

    GfxTextureDesc desc = color->GetDesc();
    desc.alloc_type = GfxAllocationType::Committed;
    desc.usage = GfxTextureUsageShared;
    desc.heap = nullptr;

    m_colorTexture.reset(device->CreateTexture(desc, "OIDN.colorTexture"));
    m_albedoTexture.reset(device->CreateTexture(desc, "OIDN.albedoTexture"));
    m_normalTexture.reset(device->CreateTexture(desc, "OIDN.normalTexture"));

    m_colorBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_colorTexture->GetSharedHandle(), nullptr, m_colorTexture->GetRequiredStagingBufferSize());
    m_albedoBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_albedoTexture->GetSharedHandle(), nullptr, m_albedoTexture->GetRequiredStagingBufferSize());
    m_normalBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE,
        m_normalTexture->GetSharedHandle(), nullptr, m_normalTexture->GetRequiredStagingBufferSize());
}

#endif // #if WITH_OIDN