#include "gfx.h"
#include "d3d12/d3d12_device.h"
#include "utils/assert.h"

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc)
{
	IGfxDevice* pDevice = nullptr;

	switch (desc.backend)
	{
	case GfxRenderBackend::D3D12:
		pDevice = new D3D12Device(desc);
		((D3D12Device*)pDevice)->Init();
		break;
	default:
		break;
	}
	
	return pDevice;
}

uint32_t GetFormatRowPitch(GfxFormat format, uint32_t width)
{
    switch (format)
    {
    case GfxFormat::RGBA32F:
    case GfxFormat::RGBA32UI:
    case GfxFormat::RGBA32SI:
        return width * 16;
    case GfxFormat::RGBA16F:
    case GfxFormat::RGBA16UI:
    case GfxFormat::RGBA16SI:
    case GfxFormat::RGBA16UNORM:
    case GfxFormat::RGBA16SNORM:
        return width * 8;
    case GfxFormat::RGBA8UI:
    case GfxFormat::RGBA8SI:
    case GfxFormat::RGBA8UNORM:
    case GfxFormat::RGBA8SNORM:
    case GfxFormat::RGBA8SRGB:
    case GfxFormat::BGRA8UNORM:
    case GfxFormat::BGRA8SRGB:
        return width * 4;
    case GfxFormat::RG32F:
    case GfxFormat::RG32UI:
    case GfxFormat::RG32SI:
        return width * 8;
    case GfxFormat::RG16F:
    case GfxFormat::RG16UI:
    case GfxFormat::RG16SI:
    case GfxFormat::RG16UNORM:
    case GfxFormat::RG16SNORM:
        return width * 4;
    case GfxFormat::RG8UI:
    case GfxFormat::RG8SI:
    case GfxFormat::RG8UNORM:
    case GfxFormat::RG8SNORM:
        return width * 2;
    case GfxFormat::R32F:
    case GfxFormat::R32UI:
    case GfxFormat::R32SI:
        return width * 4;
    case GfxFormat::R16F:
    case GfxFormat::R16UI:
    case GfxFormat::R16SI:
    case GfxFormat::R16UNORM:
    case GfxFormat::R16SNORM:
        return width * 2;
    case GfxFormat::R8UI:
    case GfxFormat::R8SI:
    case GfxFormat::R8UNORM:
    case GfxFormat::R8SNORM:
        return width;
    default:
        RE_ASSERT(false);
        return 0;
    }
}

uint32_t GetFormatBlockWidth(GfxFormat format)
{
    //todo : compressed formats
    RE_ASSERT(format <= GfxFormat::R8SNORM);
    return 1;
}

uint32_t GetFormatBlockHeight(GfxFormat format)
{
    //todo : compressed formats
    RE_ASSERT(format <= GfxFormat::R8SNORM);
    return 1;
}
