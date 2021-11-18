#include "d3d12_texture.h"
#include "d3d12_device.h"
#include "d3d12_heap.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "utils/assert.h"

static inline D3D12_RESOURCE_DESC d3d12_resource_desc(const GfxTextureDesc& desc)
{
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = desc.width;
	resourceDesc.Height = desc.height;
	resourceDesc.MipLevels = desc.mip_levels;
	resourceDesc.Format = dxgi_format(desc.format);
	resourceDesc.SampleDesc.Count = 1;

	if (desc.usage & GfxTextureUsageRenderTarget)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if (desc.usage & GfxTextureUsageDepthStencil)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if (desc.usage & GfxTextureUsageUnorderedAccess)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (!(desc.usage & GfxTextureUsageShaderResource))
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	switch (desc.type)
	{
	case GfxTextureType::Texture2D:
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = 1;
		break;
	case GfxTextureType::Texture2DArray:
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = desc.array_size;
		break;
	case GfxTextureType::Texture3D:
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		resourceDesc.DepthOrArraySize = desc.depth;
		break;
	case GfxTextureType::TextureCube:
		RE_ASSERT(desc.array_size == 6);
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = 6;
		break;
	case GfxTextureType::TextureCubeArray:
		RE_ASSERT(desc.array_size % 6 == 0);
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = desc.array_size;
		break;
	default:
		break;
	}

	return resourceDesc;
}

D3D12Texture::D3D12Texture(D3D12Device* pDevice, const GfxTextureDesc& desc, const std::string& name)
{
	m_pDevice = pDevice;
	m_desc = desc;
	m_name = name;
}

D3D12Texture::~D3D12Texture()
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;
	pDevice->Delete(m_pTexture);
	pDevice->Delete(m_pAllocation);

	for (size_t i = 0; i < m_RTV.size(); ++i)
	{
		pDevice->DeleteRTV(m_RTV[i]);
	}

	for (size_t i = 0; i < m_DSV.size(); ++i)
	{
		pDevice->DeleteDSV(m_DSV[i]);
	}
}

bool D3D12Texture::Create(D3D12Heap* heap, uint32_t offset)
{
	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

	D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();

	D3D12_RESOURCE_DESC resourceDesc = d3d12_resource_desc(m_desc);

	D3D12_RESOURCE_STATES initial_state;
	if (m_desc.usage & GfxTextureUsageRenderTarget)
	{
		initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	else if (m_desc.usage & GfxTextureUsageDepthStencil)
	{
		initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
	else if (m_desc.usage & GfxTextureUsageUnorderedAccess)
	{
		initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	else
	{
		initial_state = D3D12_RESOURCE_STATE_COMMON;
	}

	HRESULT hr;
	
	if (heap != nullptr)
	{
		RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
		RE_ASSERT(m_desc.memory_type == heap->GetDesc().memory_type);

		hr = pAllocator->CreateAliasingResource((D3D12MA::Allocation*)heap->GetHandle(), offset,
			&resourceDesc, initial_state, nullptr, IID_PPV_ARGS(&m_pTexture));
	}
	else if (m_desc.alloc_type == GfxAllocationType::Sparse)
	{
		ID3D12Device* device = (ID3D12Device*)m_pDevice->GetHandle();
		hr = device->CreateReservedResource(&resourceDesc, initial_state, nullptr, IID_PPV_ARGS(&m_pTexture));
	}
	else
	{
		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
		allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

		hr = pAllocator->CreateResource(&allocationDesc, 
			&resourceDesc, initial_state, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pTexture));
	}

	if (FAILED(hr))
	{
		return false;
	}

	std::wstring name_wstr = string_to_wstring(m_name);
	m_pTexture->SetName(name_wstr.c_str());
	if (m_pAllocation)
	{
		m_pAllocation->SetName(name_wstr.c_str());
	}

	return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetRTV(uint32_t mip_slice, uint32_t array_slice)
{
	RE_ASSERT(m_desc.usage & GfxTextureUsageRenderTarget);

	if (m_RTV.empty())
	{
		m_RTV.resize(m_desc.mip_levels * m_desc.array_size);
	}

	uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
	if (IsNullDescriptor(m_RTV[index]))
	{
		m_RTV[index] = ((D3D12Device*)m_pDevice)->AllocateRTV();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = dxgi_format(m_desc.format);

		switch (m_desc.type)
		{
		case GfxTextureType::Texture2D:
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = mip_slice;
			break;
		case GfxTextureType::Texture2DArray:
		case GfxTextureType::TextureCube:
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = mip_slice;
			rtvDesc.Texture2DArray.FirstArraySlice = array_slice;
			rtvDesc.Texture2DArray.ArraySize = 1;
			break;
		default:
			RE_ASSERT(false);
			break;
		}

		ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
		pDevice->CreateRenderTargetView(m_pTexture, &rtvDesc, m_RTV[index].cpu_handle);
	}

	return m_RTV[index].cpu_handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetDSV(uint32_t mip_slice, uint32_t array_slice)
{
	RE_ASSERT(m_desc.usage & GfxTextureUsageDepthStencil);

	if (m_DSV.empty())
	{
		m_DSV.resize(m_desc.mip_levels * m_desc.array_size);
	}

	uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
	if (IsNullDescriptor(m_DSV[index]))
	{
		m_DSV[index] = ((D3D12Device*)m_pDevice)->AllocateDSV();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = dxgi_format(m_desc.format);

		switch (m_desc.type)
		{
		case GfxTextureType::Texture2D:
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = mip_slice;
			break;
		case GfxTextureType::Texture2DArray:
		case GfxTextureType::TextureCube:
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = mip_slice;
			dsvDesc.Texture2DArray.FirstArraySlice = array_slice;
			dsvDesc.Texture2DArray.ArraySize = 1;
			break;
		default:
			RE_ASSERT(false);
			break;
		}

		ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
		pDevice->CreateDepthStencilView(m_pTexture, &dsvDesc, m_DSV[index].cpu_handle);
	}

	return m_DSV[index].cpu_handle;
}

uint32_t D3D12Texture::GetRequiredStagingBufferSize() const
{
	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

	D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();
	uint32_t subresource_count = m_desc.mip_levels * m_desc.array_size;

	uint64_t size;
	pDevice->GetCopyableFootprints(&desc, 0, subresource_count, 0, nullptr, nullptr, nullptr, &size);
	return (uint32_t)size;
}

uint32_t D3D12Texture::GetRowPitch(uint32_t mip_level) const
{
	RE_ASSERT(mip_level < m_desc.mip_levels);

	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

	D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	pDevice->GetCopyableFootprints(&desc, mip_level, 1, 0, &footprint, nullptr, nullptr, nullptr);
	return footprint.Footprint.RowPitch;
}
