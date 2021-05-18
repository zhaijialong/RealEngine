#pragma once

#include <stdint.h>
#include <string>
#include <vector>

enum class GfxRenderBackend
{
	D3D12,
	//todo : maybe Vulkan
};

enum class GfxFormat
{
	Unknown,
	
	RGBA32F,
	RGBA32UI,
	RGBA32SI,
	RGBA16F,
	RGBA16UI,
	RGBA16SI,
	RGBA16UNORM,
	RGBA16SNORM,
	RGBA8UI,
	RGBA8SI,
	RGBA8UNORM,
	RGBA8SNORM,
	RGBA8SRGB,
	BGRA8UNORM,
	BGRA8SRGB,

	RG32F,
	RG32UI,
	RG32SI,
	RG16F,
	RG16UI,
	RG16SI,
	RG16UNORM,
	RG16SNORM,
	RG8UI,
	RG8SI,
	RG8UNORM,
	RG8SNORM,

	R32F,
	R32UI,
	R32SI,
	R16F,
	R16UI,
	R16SI,
	R16UNORM,
	R16SNORM,
	R8UI,
	R8SI,
	R8UNORM,
	R8SNORM,

	D32F,
	D32FS8,
	D16,
};

enum class GfxMemoryType
{
	GpuOnly,
	CpuOnly,  //staging buffers
	CpuToGpu, //frequently updated buffers
	GpuToCpu, //readback
};

enum class GfxAllocationType
{
	Committed,
	Placed,
	//todo : SubAllocatedBuffer,
};

enum GfxBufferUsageBit
{
	GfxBufferUsageConstantBuffer    = 1 << 0,
	GfxBufferUsageStructuredBuffer  = 1 << 1,
	GfxBufferUsageTypedBuffer       = 1 << 2,
	GfxBufferUsageRawBuffer         = 1 << 3,
	GfxBufferUsageUnorderedAccess   = 1 << 4,
};
using GfxBufferUsageFlags = uint32_t;

enum GfxTextureUsageBit
{
	GfxTextureUsageShaderResource   = 1 << 0,
	GfxTextureUsageRenderTarget     = 1 << 1,
	GfxTextureUsageDepthStencil     = 1 << 2,
	GfxTextureUsageUnorderedAccess  = 1 << 3,
};
using GfxTextureUsageFlags = uint32_t;

enum class GfxTextureType
{
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCube,
	TextureCubeArray,
};

enum class GfxCommandQueue
{
	Graphics,
	Compute,
	Copy,
};

enum class GfxResourceState
{
	Common,
	RenderTarget,
	UnorderedAccess,
	DepthStencil,
	DepthStencilReadOnly,
	ShaderResource,
	ShaderResourcePSOnly,
	IndirectArg,
	CopyDst,
	CopySrc,
	ResolveDst,
	ResolveSrc,
	Present,
};

enum class GfxRenderPassLoadOp
{
	Load,
	Clear,
	DontCare,
};

enum class GfxRenderPassStoreOp
{
	Store,
	DontCare,
};

enum class GfxShaderResourceViewType
{
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCube,
	TextureCubeArray,
	StructuredBuffer,
	TypedBuffer,
	RawBuffer,
};

enum class GfxUnorderedAccessViewType
{
	Texture2D,
	Texture2DArray,
	Texture3D,
	StructuredBuffer,
	TypedBuffer,
	RawBuffer,
};

static const uint32_t GFX_ALL_SUB_RESOURCE = 0xFFFFFFFF;
static const uint32_t GFX_INVALID_RESOURCE = 0xFFFFFFFF;

struct GfxDeviceDesc
{
	GfxRenderBackend backend = GfxRenderBackend::D3D12;
	uint32_t max_frame_lag = 3;
};

struct GfxSwapchainDesc
{
	void* window_handle = nullptr;
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t backbuffer_count = 3;
	GfxFormat backbuffer_format = GfxFormat::RGBA8SRGB;
	bool enable_vsync = true;
};

struct GfxBufferDesc
{
	uint32_t stride = 1;
	uint32_t size = 1;
	GfxFormat format = GfxFormat::Unknown;
	GfxMemoryType memory_type = GfxMemoryType::GpuOnly;
	GfxAllocationType alloc_type = GfxAllocationType::Placed;
	GfxBufferUsageFlags usage = 0;
};

struct GfxTextureDesc
{
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t mip_levels = 1;
	uint32_t array_size = 1;
	GfxTextureType type = GfxTextureType::Texture2D;
	GfxFormat format = GfxFormat::Unknown;
	GfxMemoryType memory_type = GfxMemoryType::GpuOnly;
	GfxAllocationType alloc_type = GfxAllocationType::Placed;
	GfxTextureUsageFlags usage = GfxTextureUsageShaderResource;
};

struct GfxConstantBufferViewDesc
{
	uint32_t size = 0;
	uint32_t offset = 0;
};

struct GfxShaderResourceViewDesc
{
	GfxShaderResourceViewType type = GfxShaderResourceViewType::Texture2D;

	union
	{
		struct
		{
			uint32_t mip_slice = 0;
			uint32_t array_slice = 0;
			uint32_t mip_levels = uint32_t(-1);
			uint32_t array_size = 1;
			uint32_t plane_slice = 0;
		} texture;

		struct
		{
			uint32_t size = 0;
			uint32_t offset = 0;
		} buffer;
	};

	GfxShaderResourceViewDesc() : texture() {}
};

struct GfxUnorderedAccessViewDesc
{
	GfxUnorderedAccessViewType type = GfxUnorderedAccessViewType::Texture2D;

	union
	{
		struct
		{
			uint32_t mip_slice = 0;
			uint32_t array_slice = 0;
			uint32_t array_size = 1;
			uint32_t plane_slice = 0;
		} texture;

		struct
		{
			uint32_t size;
			uint32_t offset;
		} buffer;
	};
};

class IGfxTexture;

struct GfxRenderPassColorAttachment
{
	IGfxTexture* texture = nullptr;
	uint32_t mip_slice = 0;
	uint32_t array_slice = 0;
	GfxRenderPassLoadOp load_op = GfxRenderPassLoadOp::Load;
	GfxRenderPassStoreOp store_op = GfxRenderPassStoreOp::Store;
	float clear_color[4] = {};
};

struct GfxRenderPassDepthAttachment
{
	IGfxTexture* texture = nullptr;
	uint32_t mip_slice = 0;
	uint32_t array_slice = 0;
	GfxRenderPassLoadOp load_op = GfxRenderPassLoadOp::Load;
	GfxRenderPassStoreOp store_op = GfxRenderPassStoreOp::Store;
	GfxRenderPassLoadOp stencil_load_op = GfxRenderPassLoadOp::Load;
	GfxRenderPassStoreOp stencil_store_op = GfxRenderPassStoreOp::Store;
	float clear_depth = 0.0f;
	uint32_t clear_stencil = 0;
};

struct GfxRenderPassDesc
{
	GfxRenderPassColorAttachment color[8];
	GfxRenderPassDepthAttachment depth;
};

struct GfxShaderDesc
{
	std::string file;
	std::string entry_point;
	std::string profile;
	std::vector<std::string> defines;
};

enum class GfxCullMode
{
	None,
	Front,
	Back,
};

struct GfxRasterizerState
{
	GfxCullMode cull_mode = GfxCullMode::None;
	float depth_bias = 0.0f;
	float depth_bias_clamp = 0.0f;
	float depth_slope_scale = 0.0f;
	bool wireframe = false;
	bool front_ccw = false;
	bool depth_clip = true;
	bool line_aa = false;
	bool conservative_raster = false;
};

enum class GfxCompareFunc
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,
};

enum class GfxStencilOp
{
	Keep,
	Zero,
	Replace,
	IncreaseClamp,
	DecreaseClamp,
	Invert,
	IncreaseWrap,
	DecreaseWrap,
};

struct GfxDepthStencilOp
{
	GfxStencilOp stencil_fail = GfxStencilOp::Keep;
	GfxStencilOp depth_fail = GfxStencilOp::Keep;
	GfxStencilOp pass = GfxStencilOp::Keep;
	GfxCompareFunc stencil_func = GfxCompareFunc::Always;
};

struct GfxDepthStencilState
{
	GfxCompareFunc depth_func = GfxCompareFunc::Always;
	bool depth_test = false;
	bool depth_write = true;
	GfxDepthStencilOp front;
	GfxDepthStencilOp back;
	bool stencil_test = false;
	uint8_t stencil_read_mask = 0xFF;
	uint8_t stencil_write_mask = 0xFF;
};

enum class GfxBlendFactor
{
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DstAlpha,
	InvDstAlpha,
	DstColor,
	InvDstColor,
	SrcAlphaClamp,
	ConstantFactor,
	InvConstantFactor,
};

enum class GfxBlendOp
{
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max,
};

enum GfxColorWriteMaskBit
{
	GfxColorWriteMaskR = 1,
	GfxColorWriteMaskG = 2,
	GfxColorWriteMaskB = 4,
	GfxColorWriteMaskA = 8,
	GfxColorWriteMaskAll = (GfxColorWriteMaskR | GfxColorWriteMaskG | GfxColorWriteMaskB | GfxColorWriteMaskA),
};

using GfxColorWriteMask = uint8_t;

struct GfxBlendState
{
	bool blend_enable = false;
	GfxBlendFactor color_src = GfxBlendFactor::One;
	GfxBlendFactor color_dst = GfxBlendFactor::One;
	GfxBlendOp color_op = GfxBlendOp::Add;
	GfxBlendFactor alpha_src = GfxBlendFactor::One;
	GfxBlendFactor alpha_dst = GfxBlendFactor::One;
	GfxBlendOp alpha_op = GfxBlendOp::Add;
	GfxColorWriteMask write_mask = GfxColorWriteMaskAll;
};

enum class GfxPrimitiveType
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleTrip,
};

enum class GfxPipelineType
{
	Graphics,
	MeshShading,
	Compute,
	Raytracing,
};

class IGfxShader;

struct GfxGraphicsPipelineDesc
{
	IGfxShader* vs = nullptr;
	IGfxShader* ps = nullptr;
	GfxRasterizerState rasterizer_state;
	GfxDepthStencilState depthstencil_state;
	GfxBlendState blend_state[8];
	GfxFormat rt_format[8] = { GfxFormat::Unknown };
	GfxFormat depthstencil_format = GfxFormat::Unknown;
	GfxPrimitiveType primitive_type = GfxPrimitiveType::TriangleList;
};

struct GfxMeshShadingPipelineDesc
{
	IGfxShader* as = nullptr;
	IGfxShader* ms = nullptr;
	IGfxShader* ps = nullptr;
	GfxRasterizerState rasterizer_state;
	GfxDepthStencilState depthstencil_state;
	GfxBlendState blend_state[8];
	GfxFormat rt_format[8] = { GfxFormat::Unknown };
	GfxFormat depthstencil_format = GfxFormat::Unknown;
};

enum class GfxFilter
{
	Point,
	Linear,
};

enum class GfxSamplerAddressMode
{
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	ClampToBorder,
};

struct GfxSamplerDesc
{
	GfxFilter min_filter = GfxFilter::Point;
	GfxFilter mag_filter = GfxFilter::Point;
	GfxFilter mip_filter = GfxFilter::Point;
	GfxSamplerAddressMode address_u = GfxSamplerAddressMode::Repeat;
	GfxSamplerAddressMode address_v = GfxSamplerAddressMode::Repeat;
	GfxSamplerAddressMode address_w = GfxSamplerAddressMode::Repeat;
	float mip_bias = 0.0f;
	bool enable_anisotropy = false;
	float max_anisotropy = 1.0f;
	bool enable_compare = false;
	GfxCompareFunc compare_func = GfxCompareFunc::Always;
	float min_lod = -FLT_MAX;
	float max_lod = FLT_MAX;
	float border_color[4] = {};
};