#include "gfx.h"
#include "d3d12/d3d12_device.h"
#include "utils/assert.h"
#include "xxHash/xxhash.h"
#include "microprofile/microprofile.h"

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

MPRenderEvent::MPRenderEvent(IGfxCommandList* pCommandList, const std::string& event_name) :
    m_pCommandList(pCommandList)
{
    static const uint32_t EVENT_COLOR[] =
    {
        MP_LIGHTCYAN4,
        MP_SKYBLUE2,
        MP_SEAGREEN4,
        MP_LIGHTGOLDENROD4,
        MP_BROWN3,
        MP_MEDIUMPURPLE2,
        MP_SIENNA,
        MP_LIMEGREEN,
        MP_MISTYROSE,
        MP_LIGHTYELLOW,
    };

    uint32_t color_count = sizeof(EVENT_COLOR) / sizeof(EVENT_COLOR[0]);
    uint32_t color = EVENT_COLOR[XXH32(event_name.c_str(), strlen(event_name.c_str()), 0) % color_count];

    MicroProfileToken token = MicroProfileGetToken("GPU", event_name.c_str(), color, MicroProfileTokenTypeGpu);

    MICROPROFILE_GPU_ENTER_TOKEN_L(pCommandList->GetProfileLog(), token);
}

MPRenderEvent::~MPRenderEvent()
{
    MICROPROFILE_GPU_LEAVE_L(m_pCommandList->GetProfileLog());
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
    case GfxFormat::BC1UNORM:
    case GfxFormat::BC1SRGB:
    case GfxFormat::BC4UNORM:
    case GfxFormat::BC4SNORM:
        return width * 0.5;
    case GfxFormat::BC2UNORM:
    case GfxFormat::BC2SRGB:
    case GfxFormat::BC3UNORM:
    case GfxFormat::BC3SRGB:
    case GfxFormat::BC5UNORM:
    case GfxFormat::BC5SNORM:
    case GfxFormat::BC6U16F:
    case GfxFormat::BC6S16F:
    case GfxFormat::BC7UNORM:
    case GfxFormat::BC7SRGB:
        return width;
    default:
        RE_ASSERT(false);
        return 0;
    }
}

uint32_t GetFormatBlockWidth(GfxFormat format)
{
    if (format >= GfxFormat::BC1UNORM && format <= GfxFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

uint32_t GetFormatBlockHeight(GfxFormat format)
{
    if (format >= GfxFormat::BC1UNORM && format <= GfxFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

bool IsDepthFormat(GfxFormat format)
{
    return format == GfxFormat::D32FS8 || format == GfxFormat::D32F || format == GfxFormat::D16;
}
