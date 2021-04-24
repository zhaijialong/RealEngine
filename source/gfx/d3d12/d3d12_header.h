#pragma once

#include "d3d12/d3d12.h"
#include "d3d12/d3dx12.h"
#include "d3d12/dxgi1_6.h"
#include "../gfx_define.h"
#include <locale>
#include <codecvt>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if(p){p->Release(); p = nullptr;}
#endif

struct D3D12Descriptor
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {};
};

inline std::wstring string_to_wstring(std::string s)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(s);
}


inline D3D12_HEAP_TYPE d3d12_heap_type(GfxMemoryType memory_type)
{
	switch (memory_type)
	{
	case GfxMemoryType::GpuOnly:
		return D3D12_HEAP_TYPE_DEFAULT;
	case GfxMemoryType::CpuOnly:
	case GfxMemoryType::CpuToGpu:
		return D3D12_HEAP_TYPE_UPLOAD;
	case GfxMemoryType::GpuToCpu:
		return D3D12_HEAP_TYPE_READBACK;
	default:
		return D3D12_HEAP_TYPE_DEFAULT;
	}
}

inline D3D12_RESOURCE_STATES d3d12_resource_state(GfxResourceState state)
{
	switch (state)
	{
	case GfxResourceState::Common:
		return D3D12_RESOURCE_STATE_COMMON;
	case GfxResourceState::RenderTarget:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case GfxResourceState::UnorderedAccess:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case GfxResourceState::DepthStencil:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case GfxResourceState::DepthStencilReadOnly:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case GfxResourceState::ShaderResource:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	case GfxResourceState::ShaderResourcePSOnly:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case GfxResourceState::IndirectArg:
		return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	case GfxResourceState::CopyDst:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case GfxResourceState::CopySrc:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case GfxResourceState::ResolveDst:
		return D3D12_RESOURCE_STATE_RESOLVE_DEST;
	case GfxResourceState::ResolveSrc:
		return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	case GfxResourceState::Present:
		return D3D12_RESOURCE_STATE_PRESENT;
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

inline DXGI_FORMAT dxgi_format(GfxFormat format)
{
	switch (format)
	{
	case GfxFormat::Unknown:
		return DXGI_FORMAT_UNKNOWN;
	case GfxFormat::RGBA32F:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case GfxFormat::RGBA32UI:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case GfxFormat::RGBA32SI:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case GfxFormat::RGBA16F:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case GfxFormat::RGBA16UI:
		return DXGI_FORMAT_R16G16B16A16_UINT;
	case GfxFormat::RGBA16SI:
		return DXGI_FORMAT_R16G16B16A16_SINT;
	case GfxFormat::RGBA16UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case GfxFormat::RGBA16SNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case GfxFormat::RGBA8UI:
		return DXGI_FORMAT_R8G8B8A8_UINT;
	case GfxFormat::RGBA8SI:
		return DXGI_FORMAT_R8G8B8A8_SINT;
	case GfxFormat::RGBA8UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GfxFormat::RGBA8SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case GfxFormat::RGBA8SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case GfxFormat::RG32F:
		return DXGI_FORMAT_R32G32_FLOAT;
	case GfxFormat::RG32UI:
		return DXGI_FORMAT_R32G32_UINT;
	case GfxFormat::RG32SI:
		return DXGI_FORMAT_R32G32_SINT;
	case GfxFormat::RG16F:
		return DXGI_FORMAT_R16G16_FLOAT;
	case GfxFormat::RG16UI:
		return DXGI_FORMAT_R16G16_UINT;
	case GfxFormat::RG16SI:
		return DXGI_FORMAT_R16G16_SINT;
	case GfxFormat::RG16UNORM:
		return DXGI_FORMAT_R16G16_UNORM;
	case GfxFormat::RG16SNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case GfxFormat::RG8UI:
		return DXGI_FORMAT_R8G8_UINT;
	case GfxFormat::RG8SI:
		return DXGI_FORMAT_R8G8_SINT;
	case GfxFormat::RG8UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case GfxFormat::RG8SNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case GfxFormat::R32F:
		return DXGI_FORMAT_R32_FLOAT;
	case GfxFormat::R32UI:
		return DXGI_FORMAT_R32_UINT;
	case GfxFormat::R32SI:
		return DXGI_FORMAT_R32_SINT;
	case GfxFormat::R16F:
		return DXGI_FORMAT_R16_FLOAT;
	case GfxFormat::R16UI:
		return DXGI_FORMAT_R16_UINT;
	case GfxFormat::R16SI:
		return DXGI_FORMAT_R16_SINT;
	case GfxFormat::R16UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case GfxFormat::R16SNORM:
		return DXGI_FORMAT_R16_SNORM;
	case GfxFormat::R8UI:
		return DXGI_FORMAT_R8_UINT;
	case GfxFormat::R8SI:
		return DXGI_FORMAT_R8_SINT;
	case GfxFormat::R8UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case GfxFormat::R8SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case GfxFormat::D32F:
		return DXGI_FORMAT_D32_FLOAT;
	case GfxFormat::D32FS8:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case GfxFormat::D16:
		return DXGI_FORMAT_D16_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

inline D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE d3d12_render_pass_loadop(GfxRenderPassLoadOp loadOp)
{
	switch (loadOp)
	{
	case GfxRenderPassLoadOp::Load:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	case GfxRenderPassLoadOp::Clear:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	case GfxRenderPassLoadOp::DontCare:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	default:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	}
}

inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE d3d12_render_pass_storeop(GfxRenderPassStoreOp storeOp)
{
	switch (storeOp)
	{
	case GfxRenderPassStoreOp::Store:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	case GfxRenderPassStoreOp::DontCare:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
	default:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	}
}