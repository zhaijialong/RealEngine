#include "d3d12_rtv.h"
#include "d3d12_device.h"
#include "d3d12_texture.h"
#include "core/assert.h"

D3D12RTV::D3D12RTV(D3D12Device* pDevice, IGfxTexture* pTexture, const GfxRenderTargetViewDesc& desc, const std::string& name)
{
	m_pDevice = pDevice;
	m_pResource = (IGfxResource*)pTexture;
	m_desc = desc;
	m_name = name;
}

D3D12RTV::~D3D12RTV()
{
	((D3D12Device*)m_pDevice)->DeleteRtv(m_descriptor);
}

bool D3D12RTV::Create()
{
	const GfxTextureDesc& textureDesc = ((IGfxTexture*)m_pResource)->GetDesc();
	QK_ASSERT(textureDesc.usage & GfxTextureUsageRenderTarget);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = dxgi_format(textureDesc.format);

	switch (textureDesc.type)
	{
	case GfxTextureType::Texture2D:
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = m_desc.mip_slice;
		rtvDesc.Texture2D.PlaneSlice = m_desc.plane_slice;
		break;
	case GfxTextureType::Texture2DArray:
	case GfxTextureType::TextureCube:
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = m_desc.mip_slice;
		rtvDesc.Texture2DArray.PlaneSlice = m_desc.plane_slice;
		rtvDesc.Texture2DArray.FirstArraySlice = m_desc.array_slice;
		rtvDesc.Texture2DArray.ArraySize = 1;
		break;
	default:
		QK_ASSERT(false);
		break;
	}
 
	m_descriptor = ((D3D12Device*)m_pDevice)->AllocateRtv();

	ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
	pDevice->CreateRenderTargetView((ID3D12Resource*)m_pResource->GetHandle(), &rtvDesc, m_descriptor.cpu_handle);

	return true;
}
