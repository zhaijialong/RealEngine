#include "d3d12_dsv.h"
#include "d3d12_device.h"
#include "d3d12_texture.h"
#include "core/assert.h"

D3D12DSV::D3D12DSV(D3D12Device* pDevice, IGfxTexture* pTexture, const GfxDepthStencilViewDesc& desc, const std::string& name)
{
	m_pDevice = pDevice;
	m_pResource = (IGfxResource*)pTexture;
	m_desc = desc;
	m_name = name;
}

D3D12DSV::~D3D12DSV()
{
	((D3D12Device*)m_pDevice)->DeleteDsv(m_descriptor);
}

bool D3D12DSV::Create()
{
	const GfxTextureDesc& textureDesc = ((IGfxTexture*)m_pResource)->GetDesc();
	QK_ASSERT(textureDesc.usage & GfxTextureUsageDepthStencil);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = dxgi_format(textureDesc.format);

	switch (textureDesc.type)
	{
	case GfxTextureType::Texture2D:
		dsvDesc.Texture2D.MipSlice = m_desc.mip_slice;
		break;
	case GfxTextureType::Texture2DArray:
	case GfxTextureType::TextureCube:
		dsvDesc.Texture2DArray.MipSlice = m_desc.mip_slice;
		dsvDesc.Texture2DArray.FirstArraySlice = m_desc.array_slice;
		dsvDesc.Texture2DArray.ArraySize = 1;
		break;
	default:
		QK_ASSERT(false);
		break;
	}

	m_descriptor = ((D3D12Device*)m_pDevice)->AllocateDsv();

	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
	pDevice->CreateDepthStencilView((ID3D12Resource*)m_pResource->GetHandle(), &dsvDesc, m_descriptor.cpu_handle);

	return true;
}
