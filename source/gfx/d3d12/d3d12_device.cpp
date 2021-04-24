#include "d3d12_device.h"
#include "d3d12_buffer.h"
#include "d3d12_texture.h"
#include "d3d12_fence.h"
#include "d3d12_swapchain.h"
#include "d3d12_command_list.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "utils/log.h"
#include "utils/assert.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = 4; }
extern "C" { _declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

static IDXGIAdapter1* FindAdapter(IDXGIFactory4* pDXGIFactory, D3D_FEATURE_LEVEL minimumFeatureLevel)
{
	IDXGIAdapter1* pDXGIAdapter = NULL;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pDXGIFactory->EnumAdapters1(adapterIndex, &pDXGIAdapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		pDXGIAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(pDXGIAdapter, minimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}

		pDXGIAdapter->Release();
	}

	return pDXGIAdapter;
}

D3D12Device::D3D12Device(const GfxDeviceDesc& desc)
{
	m_desc = desc;
}

D3D12Device::~D3D12Device()
{
	while (!m_deletionQueue.empty())
	{
		auto item = m_deletionQueue.front();
		SAFE_RELEASE(item.object);
		m_deletionQueue.pop();
	}

	while (!m_deletionAllocationQueue.empty())
	{
		auto item = m_deletionAllocationQueue.front();
		SAFE_RELEASE(item.allocation);
		m_deletionAllocationQueue.pop();
	}

	SAFE_RELEASE(m_pResourceAllocator);
	SAFE_RELEASE(m_pGraphicsQueue);
	SAFE_RELEASE(m_pCopyQueue);

#if defined(_DEBUG)
	ID3D12DebugDevice* pDebugDevice = NULL;
	m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice));
	if (pDebugDevice)
	{
		pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
		pDebugDevice->Release();
	}
#endif

	SAFE_RELEASE(m_pDevice);
	SAFE_RELEASE(m_pDxgiAdapter);
	SAFE_RELEASE(m_pDxgiFactory);
}


IGfxBuffer* D3D12Device::CreateBuffer(const GfxBufferDesc& desc, const std::string& name)
{
	/*
	D3D12Buffer* pBuffer = new D3D12Buffer(this, desc, name);
	if (!pBuffer->Create())
	{
		delete pBuffer;
		return nullptr;
	}
	return pBuffer;
	*/
	return nullptr;
}

IGfxTexture* D3D12Device::CreateTexture(const GfxTextureDesc& desc, const std::string& name)
{
	/*
	D3D12Texture* pTexture = new D3D12Texture(this, desc, name);
	if (!pTexture->Create())
	{
		delete pTexture;
		return nullptr;
	}
	return pTexture;
	*/
	return nullptr;
}

IGfxFence* D3D12Device::CreateFence(const std::string& name)
{
	/*
	D3D12Fence* pFence = new D3D12Fence(this, name);
	if (!pFence->Create())
	{
		delete pFence;
		return nullptr;
	}
	return pFence;
	*/
	return nullptr;
}

IGfxSwapchain* D3D12Device::CreateSwapchain(const GfxSwapchainDesc& desc, void* window_handle, const std::string& name)
{
	/*
	D3D12Swapchain* pSwapchain = new D3D12Swapchain(this, desc, name);
	if (!pSwapchain->Create(window_handle))
	{
		delete pSwapchain;
		return nullptr;
	}
	return pSwapchain;
	*/
	return nullptr;
}

IGfxCommandList* D3D12Device::CreateCommandList(GfxCommandQueue queue_type, const std::string& name)
{
	/*
	D3D12CommandList* pCommandList = new D3D12CommandList(this, name);
	if (!pCommandList->Create(queue_type))
	{
		delete pCommandList;
		return nullptr;
	}
	return pCommandList;
	*/
	return nullptr;
}

IGfxRenderTargetView* D3D12Device::CreateRenderTargetView(IGfxTexture* texture, const GfxRenderTargetViewDesc& desc, const std::string& name)
{
	/*
	D3D12RTV* pRTV = new D3D12RTV(this, texture, desc, name);
	if (!pRTV->Create())
	{
		delete pRTV;
		return nullptr;
	}

	return pRTV;
	*/
	return nullptr;
}

IGfxDepthStencilView* D3D12Device::CreateDepthStencilView(IGfxTexture* texture, const GfxDepthStencilViewDesc& desc, const std::string& name)
{
/*	D3D12DSV* pDSV = new D3D12DSV(this, texture, desc, name);
	if (!pDSV->Create())
	{
		delete pDSV;
		return nullptr;
	}
	return pDSV;
	*/
	return nullptr;
}

void D3D12Device::BeginFrame()
{
	DoDeferredDeletion();
}

void D3D12Device::EndFrame()
{
	++m_nFrameID;
}

bool D3D12Device::Init()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	ID3D12Debug1* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		//debugController1->SetEnableGPUBasedValidation(TRUE);

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	SAFE_RELEASE(debugController);
#endif

	if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_pDxgiFactory))))
	{
		return false;
	}

	D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
	m_pDxgiAdapter = FindAdapter(m_pDxgiFactory, minimumFeatureLevel);
	if (m_pDxgiAdapter == nullptr)
	{
		return false;
	}
	
	if (FAILED(D3D12CreateDevice(m_pDxgiAdapter, minimumFeatureLevel, IID_PPV_ARGS(&m_pDevice))))
	{
		return false;
	}

	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel;
	shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_7;
	if (FAILED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
	{
		return false;
	}

	//SM6.6 is required !
	RE_ASSERT(shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);

	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_pDevice;
	allocatorDesc.pAdapter = m_pDxgiAdapter;
	if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_pResourceAllocator)))
	{
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pGraphicsQueue));
	m_pGraphicsQueue->SetName(L"Graphics Queue");

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCopyQueue));
	m_pCopyQueue->SetName(L"Copy Queue");

	//m_pRtvAllocator = new D3D12DescriptorPoolAllocator(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128, false);
	//m_pDsvAllocator = new D3D12DescriptorPoolAllocator(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128, false);
	//m_pCbvSrvUavAllocator = new D3D12DescriptorPoolAllocator(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, false);
	//m_pSamplerAllocator = new D3D12DescriptorPoolAllocator(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32, false);

    return true;
}

/*
D3D12Descriptor D3D12Device::AllocateCpuSrv()
{
	return m_pCbvSrvUavAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateCpuCbv()
{
	return m_pCbvSrvUavAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateCpuUav()
{
	return m_pCbvSrvUavAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateCpuSampler()
{
	return m_pSamplerAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateRtv()
{
	return m_pRtvAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateDsv()
{
	return m_pDsvAllocator->Allocate();
}

void D3D12Device::Delete(IUnknown* object)
{
	m_deletionQueue.push({ object, m_nFrameID });
}

void D3D12Device::Delete(D3D12MA::Allocation* allocation)
{
	m_deletionAllocationQueue.push({ allocation, m_nFrameID });
}

void D3D12Device::DeleteRtv(const D3D12Descriptor& descriptor)
{
}

void D3D12Device::DeleteDsv(const D3D12Descriptor& descriptor)
{
}

void D3D12Device::DeleteCpuSrv(const D3D12Descriptor& descriptor)
{
}

void D3D12Device::DeleteCpuCbv(const D3D12Descriptor& descriptor)
{
}

void D3D12Device::DeleteCpuUav(const D3D12Descriptor& descriptor)
{
}

void D3D12Device::DeleteCpuSampler(const D3D12Descriptor& descriptor)
{
}
*/

void D3D12Device::DoDeferredDeletion()
{
	while (!m_deletionQueue.empty())
	{
		auto item = m_deletionQueue.front();
		if (item.frame + m_desc.maxFrameLag > m_nFrameID)
		{
			break;
		}

		SAFE_RELEASE(item.object);
		m_deletionQueue.pop();
	}

	while (!m_deletionAllocationQueue.empty())
	{
		auto item = m_deletionAllocationQueue.front();
		if (item.frame + m_desc.maxFrameLag > m_nFrameID)
		{
			break;
		}

		SAFE_RELEASE(item.allocation);
		m_deletionAllocationQueue.pop();
	}
}
