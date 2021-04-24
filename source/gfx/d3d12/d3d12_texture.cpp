#include "d3d12_texture.h"
#include "d3d12_device.h"
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
	resourceDesc.Flags = (desc.usage & GfxTextureUsageUnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

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
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = 6;
		break;
	case GfxTextureType::TextureCubeArray:
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.DepthOrArraySize = 6 * desc.array_size;
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
}

bool D3D12Texture::Create()
{
	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

	D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
	allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

	D3D12_RESOURCE_DESC resourceDesc = d3d12_resource_desc(m_desc);

	HRESULT hr = pAllocator->CreateResource(&allocationDesc, &resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pTexture));
	if (FAILED(hr))
	{
		return false;
	}

	std::wstring name_wstr = string_to_wstring(m_name);
	m_pTexture->SetName(name_wstr.c_str());
	m_pAllocation->SetName(name_wstr.c_str());

	return true;
}

