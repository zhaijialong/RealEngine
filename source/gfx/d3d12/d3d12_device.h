#pragma once

#include "d3d12_header.h"
#include "../i_gfx_device.h"

#include <queue>

namespace D3D12MA
{
	class Allocator;
	class Allocation;
}

class D3D12DescriptorPoolAllocator;

class D3D12Device : public IGfxDevice
{
public:
	D3D12Device(const GfxDeviceDesc& desc);
	~D3D12Device();

	virtual void* GetHandle() const override { return m_pDevice; }

	virtual IGfxBuffer* CreateBuffer(const GfxBufferDesc& desc, const std::string& name) override;
	virtual IGfxTexture* CreateTexture(const GfxTextureDesc& desc, const std::string& name) override;
	virtual IGfxFence* CreateFence(const std::string& name) override;
	virtual IGfxSwapchain* CreateSwapchain(const GfxSwapchainDesc& desc, void* window_handle, const std::string& name) override;
	virtual IGfxCommandList* CreateCommandList(GfxCommandQueue queue_type, const std::string& name) override;

	virtual IGfxRenderTargetView* CreateRenderTargetView(IGfxTexture* texture, const GfxRenderTargetViewDesc& desc, const std::string& name) override;
	virtual IGfxDepthStencilView* CreateDepthStencilView(IGfxTexture* texture, const GfxDepthStencilViewDesc& desc, const std::string& name) override;

	virtual void BeginFrame() override;
	virtual void EndFrame() override;
	virtual uint64_t GetFrameID() const override { return m_nFrameID; }

	bool Init();
	IDXGIFactory4* GetDxgiFactory4() const { return m_pDxgiFactory; }
	ID3D12CommandQueue* GetGraphicsQueue() const { return m_pGraphicsQueue; }
	ID3D12CommandQueue* GetCopyQueue() const { return m_pCopyQueue; }
	D3D12MA::Allocator* GetResourceAllocator() const { return m_pResourceAllocator; }

	void Delete(IUnknown* object);
	void Delete(D3D12MA::Allocation* allocation);

	/*
	D3D12Descriptor AllocateRtv();
	D3D12Descriptor AllocateDsv();
	D3D12Descriptor AllocateCpuSrv();
	D3D12Descriptor AllocateCpuCbv();
	D3D12Descriptor AllocateCpuUav();
	D3D12Descriptor AllocateCpuSampler();

	void DeleteRtv(const D3D12Descriptor& descriptor);
	void DeleteDsv(const D3D12Descriptor& descriptor);
	void DeleteCpuSrv(const D3D12Descriptor& descriptor);
	void DeleteCpuCbv(const D3D12Descriptor& descriptor);
	void DeleteCpuUav(const D3D12Descriptor& descriptor);
	void DeleteCpuSampler(const D3D12Descriptor& descriptor);
	*/

private:
	void DoDeferredDeletion();

private:
	GfxDeviceDesc m_desc;

	IDXGIFactory4* m_pDxgiFactory = nullptr;
	IDXGIAdapter1* m_pDxgiAdapter = nullptr;

	ID3D12Device* m_pDevice = nullptr;
	ID3D12CommandQueue* m_pGraphicsQueue = nullptr;
	ID3D12CommandQueue* m_pCopyQueue = nullptr;

	D3D12MA::Allocator* m_pResourceAllocator = nullptr;

	//none shader visible descriptors
	D3D12DescriptorPoolAllocator* m_pRtvAllocator = nullptr;
	D3D12DescriptorPoolAllocator* m_pDsvAllocator = nullptr;
	D3D12DescriptorPoolAllocator* m_pCbvSrvUavAllocator = nullptr;
	D3D12DescriptorPoolAllocator* m_pSamplerAllocator = nullptr;

	uint64_t m_nFrameID = 0;

	struct DeletionObject
	{
		IUnknown* object;
		uint64_t frame;
	};
	std::queue<DeletionObject> m_deletionQueue;

	struct DeletionAllocation
	{
		D3D12MA::Allocation* allocation;
		uint64_t frame;
	};
	std::queue<DeletionAllocation> m_deletionAllocationQueue;
};