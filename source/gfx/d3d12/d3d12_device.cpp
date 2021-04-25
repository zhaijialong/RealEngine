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
	DoDeferredDeletion(true);

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
	D3D12Buffer* pBuffer = new D3D12Buffer(this, desc, name);
	if (!pBuffer->Create())
	{
		delete pBuffer;
		return nullptr;
	}
	return pBuffer;
}

IGfxTexture* D3D12Device::CreateTexture(const GfxTextureDesc& desc, const std::string& name)
{
	D3D12Texture* pTexture = new D3D12Texture(this, desc, name);
	if (!pTexture->Create())
	{
		delete pTexture;
		return nullptr;
	}
	return pTexture;
}

IGfxFence* D3D12Device::CreateFence(const std::string& name)
{
	D3D12Fence* pFence = new D3D12Fence(this, name);
	if (!pFence->Create())
	{
		delete pFence;
		return nullptr;
	}
	return pFence;
}

IGfxSwapchain* D3D12Device::CreateSwapchain(const GfxSwapchainDesc& desc, const std::string& name)
{
	D3D12Swapchain* pSwapchain = new D3D12Swapchain(this, desc, name);
	if (!pSwapchain->Create())
	{
		delete pSwapchain;
		return nullptr;
	}
	return pSwapchain;
}

IGfxCommandList* D3D12Device::CreateCommandList(GfxCommandQueue queue_type, const std::string& name)
{
	D3D12CommandList* pCommandList = new D3D12CommandList(this, queue_type, name);
	if (!pCommandList->Create())
	{
		delete pCommandList;
		return nullptr;
	}
	return pCommandList;
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

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pGraphicsQueue));
	m_pGraphicsQueue->SetName(L"Graphics Queue");

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCopyQueue));
	m_pCopyQueue->SetName(L"Copy Queue");

	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_pDevice;
	allocatorDesc.pAdapter = m_pDxgiAdapter;
	if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_pResourceAllocator)))
	{
		return false;
	}

	m_pRTVAllocator = std::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512);
	m_pDSVAllocator = std::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
	m_pResDescriptorAllocator = std::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536);
    m_pSamplerAllocator = std::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);

    return true;
}

void D3D12Device::Delete(IUnknown* object)
{
	m_deletionQueue.push({ object, m_nFrameID });
}

void D3D12Device::Delete(D3D12MA::Allocation* allocation)
{
	m_allocationDeletionQueue.push({ allocation, m_nFrameID });
}

D3D12Descriptor D3D12Device::AllocateRTV()
{
	return m_pRTVAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateDSV()
{
	return m_pDSVAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateResourceDescriptor()
{
	return m_pResDescriptorAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateSampler()
{
	return m_pSamplerAllocator->Allocate();
}

void D3D12Device::DeleteRTV(const D3D12Descriptor& descriptor)
{
	if (!IsNullDescriptor(descriptor))
	{
		m_rtvDeletionQueue.push({ descriptor, m_nFrameID });
	}
}

void D3D12Device::DeleteDSV(const D3D12Descriptor& descriptor)
{
	if (!IsNullDescriptor(descriptor))
	{
		m_dsvDeletionQueue.push({ descriptor, m_nFrameID });
	}
}

void D3D12Device::DeleteResourceDescriptor(const D3D12Descriptor& descriptor)
{
	if (!IsNullDescriptor(descriptor))
	{
		m_resourceDeletionQueue.push({ descriptor, m_nFrameID });
	}
}

void D3D12Device::DeleteSampler(const D3D12Descriptor& descriptor)
{
	if (!IsNullDescriptor(descriptor))
	{
		m_samplerDeletionQueue.push({ descriptor, m_nFrameID });
	}
}

void D3D12Device::DoDeferredDeletion(bool force_delete)
{
	while (!m_deletionQueue.empty())
	{
		auto item = m_deletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		SAFE_RELEASE(item.object);
		m_deletionQueue.pop();
	}

	while (!m_allocationDeletionQueue.empty())
	{
		auto item = m_allocationDeletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		SAFE_RELEASE(item.allocation);
		m_allocationDeletionQueue.pop();
	}

	while (!m_rtvDeletionQueue.empty())
	{
		auto item = m_rtvDeletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		m_pRTVAllocator->Free(item.descriptor);
		m_rtvDeletionQueue.pop();
	}

	while (!m_dsvDeletionQueue.empty())
	{
		auto item = m_dsvDeletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		m_pDSVAllocator->Free(item.descriptor);
		m_dsvDeletionQueue.pop();
	}

	while (!m_resourceDeletionQueue.empty())
	{
		auto item = m_resourceDeletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		m_pResDescriptorAllocator->Free(item.descriptor);
		m_resourceDeletionQueue.pop();
	}

	while (!m_samplerDeletionQueue.empty())
	{
		auto item = m_samplerDeletionQueue.front();
		if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
		{
			break;
		}

		m_pSamplerAllocator->Free(item.descriptor);
		m_samplerDeletionQueue.pop();
	}
}

D3D12DescriptorAllocator::D3D12DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count)
{
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
	m_descirptorCount = descriptor_count;
	m_allocatedCount = 0;
	m_bShaderVisible = (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = descriptor_count;
	if (m_bShaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pHeap));
}

D3D12DescriptorAllocator::~D3D12DescriptorAllocator()
{
	m_pHeap->Release();
}

D3D12Descriptor D3D12DescriptorAllocator::Allocate()
{
	if (!m_freeDescriptors.empty())
	{
		D3D12Descriptor descriptor = m_freeDescriptors.back();
		m_freeDescriptors.pop_back();
		return descriptor;
	}
	
	RE_ASSERT(m_allocatedCount <= m_descirptorCount);

	D3D12Descriptor descriptor;
	descriptor.cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pHeap->GetCPUDescriptorHandleForHeapStart(), m_allocatedCount, m_descriptorSize);
	
	if (m_bShaderVisible)
	{
		descriptor.gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pHeap->GetGPUDescriptorHandleForHeapStart(), m_allocatedCount, m_descriptorSize);
	}

	m_allocatedCount++;

	return descriptor;
}

void D3D12DescriptorAllocator::Free(const D3D12Descriptor& descriptor)
{
	m_freeDescriptors.push_back(descriptor);
}
