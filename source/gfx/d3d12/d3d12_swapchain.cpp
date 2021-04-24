#include "d3d12_swapchain.h"
#include "d3d12_device.h"
#include "d3d12_texture.h"
#include "d3d12_rtv.h"
#include "../../core/assert.h"
#include "../../core/mem_alloc.h"

D3D12Swapchain::D3D12Swapchain(D3D12Device* pDevice, const GfxSwapchainDesc& desc, const std::string& name)
{
	m_pDevice = pDevice;
	m_desc = desc;
	m_name = name;
}

D3D12Swapchain::~D3D12Swapchain()
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;
	pDevice->Delete(m_pSwapChain);

	for (size_t i = 0; i < m_backBuffers.size(); ++i)
	{
		QK_DELETE m_backBuffers[i];
	}
}

bool D3D12Swapchain::Present()
{
	HRESULT hr = m_pSwapChain->Present(m_desc.enable_vsync ? 1 : 0, 0);

	m_nCurrentBackBuffer = (m_nCurrentBackBuffer + 1) % m_desc.backbuffer_count;

	return SUCCEEDED(hr);
}

bool D3D12Swapchain::Resize(uint32_t width, uint32_t height)
{
	//todo
	return false;
}

const GfxSwapchainDesc& D3D12Swapchain::GetDesc() const
{
	return m_desc;
}

IGfxTexture* D3D12Swapchain::GetBackBuffer() const
{
	return m_backBuffers[m_nCurrentBackBuffer];
}

IGfxRenderTargetView* D3D12Swapchain::GetRenderTargetView() const
{
	return m_RTVs[m_nCurrentBackBuffer];
}

bool D3D12Swapchain::Create(void* window_handle)
{
	D3D12Device* pDevice = (D3D12Device*)m_pDevice;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = m_desc.backbuffer_count;
	swapChainDesc.Width = m_desc.width;
	swapChainDesc.Height = m_desc.height;
	swapChainDesc.Format = m_desc.backbuffer_format == GfxFormat::RGBA8SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : dxgi_format(m_desc.backbuffer_format);
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	IDXGIFactory4* pFactory = pDevice->GetDxgiFactory4();
	IDXGISwapChain1* pSwapChain = NULL;
	HRESULT hr = pFactory->CreateSwapChainForHwnd(
		pDevice->GetGraphicsQueue(), // Swap chain needs the queue so that it can force a flush on it.
		(HWND)window_handle,
		&swapChainDesc,
		nullptr,
		nullptr,
		&pSwapChain);

	if (FAILED(hr))
	{
		return false;
	}

	pSwapChain->QueryInterface(&m_pSwapChain);
	SAFE_RELEASE(pSwapChain);

	GfxTextureDesc textureDesc;
	textureDesc.width = m_desc.width;
	textureDesc.height = m_desc.height;
	textureDesc.format = m_desc.backbuffer_format;
	textureDesc.usage = GfxTextureUsageRenderTarget;

	GfxRenderTargetViewDesc rtvDesc;

	for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
	{
		ID3D12Resource* pBackbuffer = NULL;
		hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackbuffer));
		if (FAILED(hr))
		{
			return false;
		}

		D3D12Texture* texture = QK_NEW D3D12Texture(pDevice, textureDesc, m_name + " texture " + std::to_string(i));
		texture->m_pTexture = pBackbuffer;
		m_backBuffers.push_back(texture);

		IGfxRenderTargetView* rtv = pDevice->CreateRenderTargetView(texture, rtvDesc, texture->GetName() + " RTV");
		m_RTVs.push_back(rtv);
	}

	return true;
}
