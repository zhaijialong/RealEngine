#include "oidn.h"
#include "renderer.h"
#include "utils/log.h"
#include "magic_enum/magic_enum.hpp"

static void OnErrorCallback(void* userPtr, OIDNError code, const char* message)
{
    RE_ERROR("{} : {}", magic_enum::enum_name(code), message);
}

OIDN::OIDN(Renderer* renderer)
{
    m_renderer = renderer;
    m_fence.reset(renderer->GetDevice()->CreateFence("OIDN.fence"));

    m_device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    if (m_device)
    {
        oidnSetDeviceErrorFunction(m_device, OnErrorCallback, nullptr);
        oidnCommitDevice(m_device);

        m_filter = oidnNewFilter(m_device, "RT");
    }
}

OIDN::~OIDN()
{
    oidnReleaseBuffer(m_colorBuffer);
    oidnReleaseBuffer(m_albedoBuffer);
    oidnReleaseBuffer(m_normalBuffer);
    oidnReleaseFilter(m_filter);
    oidnReleaseDevice(m_device);
}

void OIDN::Execute(IGfxCommandList* commandList, IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    InitBuffers(color, albedo, normal);

    if (!m_bDenoised)
    {
        commandList->TextureBarrier(color, 0, GfxAccessCopyDst, GfxAccessCopySrc);
        commandList->CopyTexture(m_colorTexture.get(), 0, 0, color, 0, 0);

        commandList->End();
        commandList->Signal(m_fence.get(), ++m_fenceValue);
        commandList->Submit();
        m_fence->Wait(m_fenceValue); // to ensure the copy has finished

        oidnExecuteFilter(m_filter);
        m_bDenoised = true;

        commandList->Begin();
        commandList->TextureBarrier(color, 0, GfxAccessCopySrc, GfxAccessCopyDst);
        m_renderer->SetupGlobalConstants(commandList);
    }

    commandList->CopyTexture(color, 0, 0, m_colorTexture.get(), 0, 0);
}

void OIDN::InitBuffers(IGfxTexture* color, IGfxTexture* albedo, IGfxTexture* normal)
{
    const uint32_t width = color->GetDesc().width;
    const uint32_t height = color->GetDesc().height;

    if (m_colorTexture == nullptr ||
        m_colorTexture->GetDesc().width != width ||
        m_colorTexture->GetDesc().height != height)
    {
        IGfxDevice* device = m_renderer->GetDevice();

        GfxTextureDesc desc = color->GetDesc();
        desc.alloc_type = GfxAllocationType::Committed;
        desc.usage = GfxTextureUsageShared;
        desc.heap = nullptr;
        
        m_colorTexture.reset(device->CreateTexture(desc, "OIDN.color"));
        m_colorBuffer = oidnNewSharedBufferFromWin32Handle(m_device, OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE, 
            m_colorTexture->GetHandle(), nullptr, color->GetRequiredStagingBufferSize());

        oidnSetFilterImage(m_filter, "color", m_colorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, m_colorTexture->GetRowPitch(0));
        oidnSetFilterImage(m_filter, "output", m_colorBuffer, OIDN_FORMAT_HALF3, width, height, 0, 8, m_colorTexture->GetRowPitch(0));

        //todo "albedo", "normal"

        oidnSetFilterBool(m_filter, "hdr", true);
        oidnSetFilterBool(m_filter, "cleanAux", true);
        oidnCommitFilter(m_filter);
    }
}
