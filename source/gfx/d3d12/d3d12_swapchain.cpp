#include "d3d12_swapchain.h"
#include "d3d12_device.h"
#include "d3d12_texture.h"
#include "utils/assert.h"
#include "fmt/format.h"

D3D12Swapchain::D3D12Swapchain(D3D12Device* pDevice, const GfxSwapchainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Swapchain::~D3D12Swapchain()
{
    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();

    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pSwapChain);
}

bool D3D12Swapchain::Present()
{
    UINT interval, flags;
    if (m_bEnableVsync)
    {
        interval = 1;
        flags = 0;
    }
    else
    {
        interval = 0;
        flags = m_bSupportTearing && m_bWindowMode ? DXGI_PRESENT_ALLOW_TEARING : 0;
    }

    HRESULT hr = m_pSwapChain->Present(interval, flags);

    m_nCurrentBackBuffer = (m_nCurrentBackBuffer + 1) % m_desc.backbuffer_count;

    return SUCCEEDED(hr);
}

bool D3D12Swapchain::Resize(uint32_t width, uint32_t height)
{
    if (m_desc.width == width && m_desc.height == height)
    {
        return false;
    }

    m_desc.width = width;
    m_desc.height = height;
    m_nCurrentBackBuffer = 0;

    for (size_t i = 0; i < m_backBuffers.size(); ++i)
    {
        delete m_backBuffers[i];
    }
    m_backBuffers.clear();

    ((D3D12Device*)m_pDevice)->FlushDeferredDeletions();

    DXGI_SWAP_CHAIN_DESC desc = {};
    m_pSwapChain->GetDesc(&desc);
    HRESULT hr = m_pSwapChain->ResizeBuffers(m_desc.backbuffer_count, width, height, desc.BufferDesc.Format, desc.Flags);
    if (!SUCCEEDED(hr))
    {
        return false;
    }

    BOOL fullscreenState;
    m_pSwapChain->GetFullscreenState(&fullscreenState, nullptr);
    m_bWindowMode = !fullscreenState;

    return CreateTextures();
}

IGfxTexture* D3D12Swapchain::GetBackBuffer() const
{
    return m_backBuffers[m_nCurrentBackBuffer];
}

bool D3D12Swapchain::Create()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    IDXGIFactory5* pFactory = pDevice->GetDxgiFactory();

    BOOL allowTearing = FALSE;
    pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    m_bSupportTearing = allowTearing;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = m_desc.backbuffer_count;
    swapChainDesc.Width = m_desc.width;
    swapChainDesc.Height = m_desc.height;
    swapChainDesc.Format = m_desc.backbuffer_format == GfxFormat::RGBA8SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : dxgi_format(m_desc.backbuffer_format);
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    IDXGISwapChain1* pSwapChain = NULL;
    HRESULT hr = pFactory->CreateSwapChainForHwnd(
        pDevice->GetGraphicsQueue(), // Swap chain needs the queue so that it can force a flush on it.
        (HWND)m_desc.window_handle,
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

    return CreateTextures();
}

bool D3D12Swapchain::CreateTextures()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;

    GfxTextureDesc textureDesc;
    textureDesc.width = m_desc.width;
    textureDesc.height = m_desc.height;
    textureDesc.format = m_desc.backbuffer_format;
    textureDesc.usage = GfxTextureUsageRenderTarget;

    for (uint32_t i = 0; i < m_desc.backbuffer_count; ++i)
    {
        ID3D12Resource* pBackbuffer = NULL;
        HRESULT hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackbuffer));
        if (FAILED(hr))
        {
            return false;
        }

        eastl::string name = fmt::format("{} texture {}", m_name.c_str(), i).c_str();
        pBackbuffer->SetName(string_to_wstring(name).c_str());

        D3D12Texture* texture = new D3D12Texture(pDevice, textureDesc, name);
        texture->m_pTexture = pBackbuffer;
        m_backBuffers.push_back(texture);
    }

    return true;
}
