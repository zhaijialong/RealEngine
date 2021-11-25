#pragma once

#include "d3d12_header.h"
#include "../i_gfx_swapchain.h"
#include <vector>

class D3D12Device;

class D3D12Swapchain : public IGfxSwapchain
{
public:
    D3D12Swapchain(D3D12Device* pDevice, const GfxSwapchainDesc& desc, const std::string& name);
    ~D3D12Swapchain();

    virtual void* GetHandle() const override { return m_pSwapChain; }
    virtual bool Present() override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual IGfxTexture* GetBackBuffer() const override;

    bool Create();

private:
    bool CreateTextures();

private:
    IDXGISwapChain3* m_pSwapChain = nullptr;

    uint32_t m_nCurrentBackBuffer = 0;
    std::vector<IGfxTexture*> m_backBuffers;
};