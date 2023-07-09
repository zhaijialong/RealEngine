#include "d3d12_device.h"
#include "d3d12_buffer.h"
#include "d3d12_texture.h"
#include "d3d12_fence.h"
#include "d3d12_swapchain.h"
#include "d3d12_command_list.h"
#include "d3d12_shader.h"
#include "d3d12_pipeline_state.h"
#include "d3d12_descriptor.h"
#include "d3d12_heap.h"
#include "d3d12_rt_blas.h"
#include "d3d12_rt_tlas.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "pix_runtime.h"
#include "ags.h"
#include "utils/log.h"
#include "utils/profiler.h"
#include "utils/math.h"
#include "magic_enum/magic_enum.hpp"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = 711; }
extern "C" { _declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

static void __stdcall D3D12MessageCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
{
    RE_DEBUG(pDescription);
}

static IDXGIAdapter1* FindAdapter(IDXGIFactory6* pDXGIFactory, D3D_FEATURE_LEVEL minimumFeatureLevel)
{
    eastl::vector<IDXGIAdapter1*> adapters;

    RE_DEBUG("available GPUs :");
    IDXGIAdapter1* pDXGIAdapter = nullptr;
    for (UINT adapterIndex = 0; 
        DXGI_ERROR_NOT_FOUND != pDXGIFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pDXGIAdapter));
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        pDXGIAdapter->GetDesc1(&desc);
        RE_DEBUG("  - {}", wstring_to_string(desc.Description));

        adapters.push_back(pDXGIAdapter);
    }

    auto selectedAdapter = eastl::find_if(adapters.begin(), adapters.end(), [=](IDXGIAdapter1* adapter)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            bool isSoftwareGPU = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
            bool capableFeatureLevel = SUCCEEDED(D3D12CreateDevice(adapter, minimumFeatureLevel, _uuidof(ID3D12Device), nullptr));

            return !isSoftwareGPU && capableFeatureLevel;
        });

    for (auto adapter : adapters)
    {
        if (selectedAdapter == nullptr || adapter != *selectedAdapter)
        {
            adapter->Release();
        }
    }

    return selectedAdapter == nullptr ? nullptr : *selectedAdapter;
}

D3D12Device::D3D12Device(const GfxDeviceDesc& desc)
{
    m_desc = desc;
}

D3D12Device::~D3D12Device()
{
    for (uint32_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBufferAllocators[i].reset();
    }

    FlushDeferredDeletions();

    m_pRTVAllocator.reset();
    m_pDSVAllocator.reset();
    m_pResDescriptorAllocator.reset();
    m_pSamplerAllocator.reset();
    m_pNonShaderVisibleUavAllocator.reset();
    
    SAFE_RELEASE(m_pDrawSignature);
    SAFE_RELEASE(m_pDrawIndexedSignature);
    SAFE_RELEASE(m_pDispatchSignature);
    SAFE_RELEASE(m_pDispatchMeshSignature);
    SAFE_RELEASE(m_pMultiDrawSignature);
    SAFE_RELEASE(m_pMultiDrawIndexedSignature);
    SAFE_RELEASE(m_pMultiDispatchSignature);
    SAFE_RELEASE(m_pMultiDispatchMeshSignature);
    SAFE_RELEASE(m_pRootSignature);
    SAFE_RELEASE(m_pResourceAllocator);
    SAFE_RELEASE(m_pGraphicsQueue);
    SAFE_RELEASE(m_pComputeQueue);
    SAFE_RELEASE(m_pCopyQueue);
    SAFE_RELEASE(m_pDxgiAdapter);
    SAFE_RELEASE(m_pDxgiFactory);

#if defined(_DEBUG)
    ID3D12DebugDevice* pDebugDevice = NULL;
    m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice));
#endif

    if (m_vendor == GfxVendor::AMD)
    {
        ags::ReleaseDevice(m_pDevice);
    }
    else
    {
        SAFE_RELEASE(m_pDevice);
    }

#if defined(_DEBUG)
    if (pDebugDevice)
    {
        pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        pDebugDevice->Release();
    }
#endif
}


IGfxBuffer* D3D12Device::CreateBuffer(const GfxBufferDesc& desc, const eastl::string& name)
{
    D3D12Buffer* pBuffer = new D3D12Buffer(this, desc, name);
    if (!pBuffer->Create())
    {
        delete pBuffer;
        return nullptr;
    }
    return pBuffer;
}

IGfxTexture* D3D12Device::CreateTexture(const GfxTextureDesc& desc, const eastl::string& name)
{
    D3D12Texture* pTexture = new D3D12Texture(this, desc, name);
    if (!pTexture->Create())
    {
        delete pTexture;
        return nullptr;
    }
    return pTexture;
}

IGfxFence* D3D12Device::CreateFence(const eastl::string& name)
{
    D3D12Fence* pFence = new D3D12Fence(this, name);
    if (!pFence->Create())
    {
        delete pFence;
        return nullptr;
    }
    return pFence;
}

IGfxHeap* D3D12Device::CreateHeap(const GfxHeapDesc& desc, const eastl::string& name)
{
    D3D12Heap* pHeap = new D3D12Heap(this, desc, name);
    if (!pHeap->Create())
    {
        delete pHeap;
        return nullptr;
    }
    return pHeap;
}

IGfxSwapchain* D3D12Device::CreateSwapchain(const GfxSwapchainDesc& desc, const eastl::string& name)
{
    D3D12Swapchain* pSwapchain = new D3D12Swapchain(this, desc, name);
    if (!pSwapchain->Create())
    {
        delete pSwapchain;
        return nullptr;
    }
    return pSwapchain;
}

IGfxCommandList* D3D12Device::CreateCommandList(GfxCommandQueue queue_type, const eastl::string& name)
{
    D3D12CommandList* pCommandList = new D3D12CommandList(this, queue_type, name);
    if (!pCommandList->Create())
    {
        delete pCommandList;
        return nullptr;
    }
    return pCommandList;
}

IGfxShader* D3D12Device::CreateShader(const GfxShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    return new D3D12Shader(this, desc, data, name);
}

IGfxPipelineState* D3D12Device::CreateGraphicsPipelineState(const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    D3D12GraphicsPipelineState* pPipeline = new D3D12GraphicsPipelineState(this, desc, name);
    if (!pPipeline->Create())
    {
        delete pPipeline;
        return nullptr;
    }
    return pPipeline;
}

IGfxPipelineState* D3D12Device::CreateMeshShadingPipelineState(const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    D3D12MeshShadingPipelineState* pPipeline = new D3D12MeshShadingPipelineState(this, desc, name);
    if (!pPipeline->Create())
    {
        delete pPipeline;
        return nullptr;
    }
    return pPipeline;
}

IGfxPipelineState* D3D12Device::CreateComputePipelineState(const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    D3D12ComputePipelineState* pPipeline = new D3D12ComputePipelineState(this, desc, name);
    if (!pPipeline->Create())
    {
        delete pPipeline;
        return nullptr;
    }
    return pPipeline;
}

IGfxDescriptor* D3D12Device::CreateShaderResourceView(IGfxResource* resource, const GfxShaderResourceViewDesc& desc, const eastl::string& name)
{
    D3D12ShaderResourceView* pSRV = new D3D12ShaderResourceView(this, resource, desc, name);
    if (!pSRV->Create())
    {
        delete pSRV;
        return nullptr;
    }
    return pSRV;
}

IGfxDescriptor* D3D12Device::CreateUnorderedAccessView(IGfxResource* resource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    D3D12UnorderedAccessView* pUAV = new D3D12UnorderedAccessView(this, resource, desc, name);
    if (!pUAV->Create())
    {
        delete pUAV;
        return nullptr;
    }
    return pUAV;
}

IGfxDescriptor* D3D12Device::CreateConstantBufferView(IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name)
{
    D3D12ConstantBufferView* pCBV = new D3D12ConstantBufferView(this, buffer, desc, name);
    if (!pCBV->Create())
    {
        delete pCBV;
        return nullptr;
    }
    return pCBV;
}

IGfxDescriptor* D3D12Device::CreateSampler(const GfxSamplerDesc& desc, const eastl::string& name)
{
    D3D12Sampler* pSampler = new D3D12Sampler(this, desc, name);
    if (!pSampler->Create())
    {
        delete pSampler;
        return nullptr;
    }
    return pSampler;
}

IGfxRayTracingBLAS* D3D12Device::CreateRayTracingBLAS(const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    D3D12RayTracingBLAS* blas = new D3D12RayTracingBLAS(this, desc, name);
    if (!blas->Create())
    {
        delete blas;
        return nullptr;
    }
    return blas;
}

IGfxRayTracingTLAS* D3D12Device::CreateRayTracingTLAS(const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    D3D12RayTracingTLAS* tlas = new D3D12RayTracingTLAS(this, desc, name);
    if (!tlas->Create())
    {
        delete tlas;
        return nullptr;
    }
    return tlas;
}

uint32_t D3D12Device::GetAllocationSize(const GfxTextureDesc& desc)
{
    D3D12_RESOURCE_DESC resourceDesc = d3d12_resource_desc(desc);
    D3D12_RESOURCE_ALLOCATION_INFO info = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);

    return (uint32_t)info.SizeInBytes;
}

bool D3D12Device::DumpMemoryStats(const eastl::string& file)
{
    FILE* f = nullptr;
    _wfopen_s(&f, string_to_wstring(file).c_str(), L"wb");
    if (f == nullptr)
    {
        return false;
    }

    WCHAR* pStatsString = nullptr;
    m_pResourceAllocator->BuildStatsString(&pStatsString, true);

    fwrite(pStatsString, 1, wcslen(pStatsString) * sizeof(WCHAR), f);
    fclose(f);

    m_pResourceAllocator->FreeStatsString(pStatsString);
    return true;
}

void D3D12Device::BeginFrame()
{
    DoDeferredDeletion();

    uint32_t index = m_nFrameID % GFX_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocators[index]->Reset();
}

void D3D12Device::EndFrame()
{
    ++m_nFrameID;

    m_pResourceAllocator->SetCurrentFrameIndex((UINT)m_nFrameID);
}

bool D3D12Device::Init()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    ID3D12Debug1* debugController = nullptr;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        //debugController->SetEnableGPUBasedValidation(TRUE);

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
        RE_ERROR("failed to find a capable DXGI adapter.");
        return false;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    m_pDxgiAdapter->GetDesc(&adapterDesc);
    switch (adapterDesc.VendorId)
    {
    case 0x1002:
        m_vendor = GfxVendor::AMD;
        break;
    case 0x10DE:
        m_vendor = GfxVendor::Nvidia;
        break;
    case 0x8086:
        m_vendor = GfxVendor::Intel;
        break;
    default:
        break;
    }

    RE_INFO("Vendor : {}", magic_enum::enum_name(m_vendor));
    RE_INFO("GPU : {}", wstring_to_string(adapterDesc.Description));

    if (m_vendor == GfxVendor::AMD)
    {
        if (FAILED(ags::CreateDevice(m_pDxgiAdapter, minimumFeatureLevel, IID_PPV_ARGS(&m_pDevice))))
        {
            return false;
        }
    }
    else
    {
        if (FAILED(D3D12CreateDevice(m_pDxgiAdapter, minimumFeatureLevel, IID_PPV_ARGS(&m_pDevice))))
        {
            return false;
        }
    }

    m_featureSupport.Init(m_pDevice);

    bool capableDevice = m_featureSupport.HighestShaderModel() >= D3D_SHADER_MODEL_6_6 &&
        m_featureSupport.ResourceBindingTier() >= D3D12_RESOURCE_BINDING_TIER_3 &&
        m_featureSupport.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1 &&
        m_featureSupport.RenderPassesTier() >= D3D12_RENDER_PASS_TIER_0 &&
        m_featureSupport.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1 &&
        m_featureSupport.WaveOps() &&
        m_featureSupport.Native16BitShaderOpsSupported();

    if (!capableDevice)
    {
        RE_ERROR("the device is not capable of running RealEngine.");
        return false;
    }

#if _DEBUG
    ID3D12InfoQueue1* pInfoQueue1 = nullptr;
    m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue1));
    if (pInfoQueue1)
    {
        DWORD CallbackCookie;
        pInfoQueue1->RegisterMessageCallback(D3D12MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &CallbackCookie);
    }
    SAFE_RELEASE(pInfoQueue1);
#endif

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pGraphicsQueue));
    m_pGraphicsQueue->SetName(L"Graphics Queue");

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pComputeQueue));
    m_pComputeQueue->SetName(L"Compute Queue");

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

    for (uint32_t i = 0; i < GFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("CB Allocator {}", i).c_str();
        m_pConstantBufferAllocators[i] = eastl::make_unique<D3D12ConstantBufferAllocator>(this, 8 * 1024 * 1024, name);
    }

    m_pRTVAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false, 512, "RTV Heap");
    m_pDSVAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false, 128, "DSV Heap");
    m_pResDescriptorAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, 65536, "Resource Heap");
    m_pSamplerAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, 128, "Sampler Heap");

    m_pNonShaderVisibleUavAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false, 4096, "Non shader visible UAV Heap");

    CreateRootSignature();
    CreateIndirectCommandSignatures();

    pix::Init();

#if MICROPROFILE_GPU_TIMERS_D3D12
    m_nProfileGraphicsQueue = MicroProfileInitGpuQueue("GPU Graphics Queue");
    m_nProfileComputeQueue = MicroProfileInitGpuQueue("GPU Compute Queue");
    //m_nProfileCopyQueue = MicroProfileInitGpuQueue("GPU Copy Queue");

    MicroProfileGpuInitD3D12(m_pDevice, 1, (void**)&m_pGraphicsQueue);
    MicroProfileSetCurrentNodeD3D12(0);
#endif

    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12Device::AllocateConstantBuffer(const void* data, size_t data_size)
{
    void* cpu_address;
    uint64_t gpu_address;

    uint32_t index = m_nFrameID % GFX_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocators[index]->Allocate((uint32_t)data_size, &cpu_address, &gpu_address);

    memcpy(cpu_address, data, data_size);

    return gpu_address;
}

void D3D12Device::FlushDeferredDeletions()
{
    DoDeferredDeletion(true);
}

void D3D12Device::Delete(IUnknown* object)
{
    if (object)
    {
        m_deletionQueue.push({ object, m_nFrameID });
    }
}

void D3D12Device::Delete(D3D12MA::Allocation* allocation)
{
    if (allocation)
    {
        m_allocationDeletionQueue.push({ allocation, m_nFrameID });
    }
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

D3D12Descriptor D3D12Device::AllocateNonShaderVisibleUAV()
{
    return m_pNonShaderVisibleUavAllocator->Allocate();
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

void D3D12Device::DeleteNonShaderVisibleUAV(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_nonShaderVisibleUAVDeletionQueue.push({ descriptor, m_nFrameID });
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

    while (!m_nonShaderVisibleUAVDeletionQueue.empty())
    {
        auto item = m_nonShaderVisibleUAVDeletionQueue.front();
        if (!force_delete && item.frame + m_desc.max_frame_lag > m_nFrameID)
        {
            break;
        }

        m_pNonShaderVisibleUavAllocator->Free(item.descriptor);
        m_nonShaderVisibleUAVDeletionQueue.pop();
    }
}

void D3D12Device::CreateRootSignature()
{
    //AMD : Try to stay below 13 DWORDs https://gpuopen.com/performance/
    //8 root constants + 2 root CBVs == 12 DWORDs, everything else is bindless

    CD3DX12_ROOT_PARAMETER1 root_parameters[GFX_MAX_CBV_BINDINGS] = {};
    root_parameters[0].InitAsConstants(GFX_MAX_ROOT_CONSTANTS, 0);
    for (uint32_t i = 1; i < GFX_MAX_CBV_BINDINGS; ++i)
    {
        root_parameters[i].InitAsConstantBufferView(i);
    }

    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | //todo : does this SM6.6 flag cost additional DWORDs ?
        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(_countof(root_parameters), root_parameters, 0, nullptr, flags);

    ID3DBlob* signature = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    RE_ASSERT(SUCCEEDED(hr));

    hr = m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature));
    RE_ASSERT(SUCCEEDED(hr));

    SAFE_RELEASE(signature);
    SAFE_RELEASE(error);

    m_pRootSignature->SetName(L"D3D12Device::m_pRootSignature");
}

void D3D12Device::CreateIndirectCommandSignatures()
{
    D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};

    D3D12_COMMAND_SIGNATURE_DESC desc = {};
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &argumentDesc;

    desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    HRESULT hr = m_pDevice->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDrawSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pDrawSignature->SetName(L"D3D12Device::m_pDrawSignature");

    desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    hr = m_pDevice->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDrawIndexedSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pDrawIndexedSignature->SetName(L"D3D12Device::m_pDrawIndexedSignature");

    desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    hr = m_pDevice->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDispatchSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pDispatchSignature->SetName(L"D3D12Device::m_pDispatchSignature");

    desc.ByteStride = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    hr = m_pDevice->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDispatchMeshSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pDispatchMeshSignature->SetName(L"D3D12Device::m_pDispatchMeshSignature");

    D3D12_INDIRECT_ARGUMENT_DESC mdiArgumentDesc[2] = {};
    mdiArgumentDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; //a uint32 root constant ("gl_DrawID")
    mdiArgumentDesc[0].Constant.RootParameterIndex = 0;
    mdiArgumentDesc[0].Constant.Num32BitValuesToSet = 1;

    desc.NumArgumentDescs = 2;
    desc.pArgumentDescs = mdiArgumentDesc;

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DRAW_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    hr = m_pDevice->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDrawSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pMultiDrawSignature->SetName(L"D3D12Device::m_pMultiDrawSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    hr = m_pDevice->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDrawIndexedSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pMultiDrawIndexedSignature->SetName(L"D3D12Device::m_pMultiDrawIndexedSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DISPATCH_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    hr = m_pDevice->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDispatchSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pMultiDispatchSignature->SetName(L"D3D12Device::m_pMultiDispatchSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    hr = m_pDevice->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDispatchMeshSignature));
    RE_ASSERT(SUCCEEDED(hr));
    m_pMultiDispatchMeshSignature->SetName(L"D3D12Device::m_pMultiDispatchMeshSignature");
}

D3D12DescriptorAllocator::D3D12DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visible, uint32_t descriptor_count, const eastl::string& name)
{
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    m_descirptorCount = descriptor_count;
    m_allocatedCount = 0;
    m_bShaderVisible = shader_visible;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = descriptor_count;
    if (m_bShaderVisible)
    {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }
    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pHeap));

    m_pHeap->SetName(string_to_wstring(name).c_str());
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

    D3D12Descriptor descriptor = GetDescriptor(m_allocatedCount);

    m_allocatedCount++;

    return descriptor;
}

void D3D12DescriptorAllocator::Free(const D3D12Descriptor& descriptor)
{
    m_freeDescriptors.push_back(descriptor);
}

D3D12Descriptor D3D12DescriptorAllocator::GetDescriptor(uint32_t index) const
{
    D3D12Descriptor descriptor;
    descriptor.cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pHeap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize);

    if (m_bShaderVisible)
    {
        descriptor.gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pHeap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
    }

    descriptor.index = index;

    return descriptor;
}

D3D12ConstantBufferAllocator::D3D12ConstantBufferAllocator(D3D12Device* device, uint32_t buffer_size, const eastl::string& name)
{
    GfxBufferDesc desc;
    desc.size = buffer_size;
    desc.memory_type = GfxMemoryType::CpuToGpu;
    desc.usage = GfxBufferUsageConstantBuffer;

    m_pBuffer.reset(device->CreateBuffer(desc, name));
}

void D3D12ConstantBufferAllocator::Allocate(uint32_t size, void** cpu_address, uint64_t* gpu_address)
{
    RE_ASSERT(m_allcatedSize + size <= m_pBuffer->GetDesc().size);

    *cpu_address = (char*)m_pBuffer->GetCpuAddress() + m_allcatedSize;
    *gpu_address = m_pBuffer->GetGpuAddress() + m_allcatedSize;

    m_allcatedSize += RoundUpPow2(size, 256); //alignment be a multiple of 256
}

void D3D12ConstantBufferAllocator::Reset()
{
    m_allcatedSize = 0;
}
