#include "shader_compiler.h"
#include "renderer.h"
#include "shader_cache.h"
#include "core/engine.h"
#include "utils/log.h"
#include "utils/string.h"
#include "utils/assert.h"

#include <filesystem>
#if RE_PLATFORM_WINDOWS
#include <atlbase.h> //CComPtr
#endif
#include "dxc/dxcapi.h"

class DXCIncludeHandler : public IDxcIncludeHandler
{
public:
    DXCIncludeHandler(ShaderCache* pShaderCache, IDxcUtils* pDxcUtils) : m_pShaderCache(pShaderCache), m_pDxcUtils(pDxcUtils)
    {
    }

    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override
    {
        eastl::string absolute_path = std::filesystem::absolute(fileName).string().c_str();
        eastl::string source = m_pShaderCache->GetCachedFileContent(absolute_path);

        *includeSource = nullptr;
        return m_pDxcUtils->CreateBlob(source.data(), (UINT32)source.size(), CP_UTF8, reinterpret_cast<IDxcBlobEncoding**>(includeSource));
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        ++m_ref;
        return m_ref;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        --m_ref;
        ULONG result = m_ref;
        if (result == 0)
        {
            delete this;
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (IsEqualIID(iid, __uuidof(IDxcIncludeHandler)))
        {
            *object = dynamic_cast<IDxcIncludeHandler*>(this);
            this->AddRef();
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(IUnknown)))
        {
            *object = dynamic_cast<IUnknown*>(this);
            this->AddRef();
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

private:
    ShaderCache* m_pShaderCache = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    std::atomic<ULONG> m_ref = 0;
};

ShaderCompiler::ShaderCompiler(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
#if RE_PLATFORM_WINDOWS
    HMODULE dxc = LoadLibrary(L"dxcompiler.dll");
#else
    eastl::string lib = Engine::GetInstance()->GetWorkPath() + "libdxcompiler.dylib";
    void* dxc = dlopen(lib.c_str(), RTLD_LAZY);
#endif
    if (dxc)
    {
#if RE_PLATFORM_WINDOWS
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc, "DxcCreateInstance");
#else
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)dlsym(dxc, "DxcCreateInstance");
#endif

        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_pDxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_pDxcCompiler));

        //m_pDxcUtils->CreateDefaultIncludeHandler(&m_pDxcIncludeHandler);
        m_pDxcIncludeHandler = new DXCIncludeHandler(pRenderer->GetShaderCache(), m_pDxcUtils);
        m_pDxcIncludeHandler->AddRef();
    }
    
#if RE_PLATFORM_MAC
    CreateMetalCompiler();
#endif
}

ShaderCompiler::~ShaderCompiler()
{
    if(m_pDxcIncludeHandler)
    {
        m_pDxcIncludeHandler->Release();
    }
    
    if(m_pDxcCompiler)
    {
        m_pDxcCompiler->Release();
    }
    
    if(m_pDxcUtils)
    {
        m_pDxcUtils->Release();
    }
    
#if RE_PLATFORM_MAC
    DestroyMetalCompiler();
#endif
}

inline const wchar_t* GetShaderProfile(GfxShaderType type)
{
    switch (type)
    {
    case GfxShaderType::AS:
        return L"as_6_6";
    case GfxShaderType::MS:
        return L"ms_6_6";
    case GfxShaderType::VS:
        return L"vs_6_6";
    case GfxShaderType::PS:
        return L"ps_6_6";
    case GfxShaderType::CS:
        return L"cs_6_6";
    default:
        return L"";
    }
}

bool ShaderCompiler::Compile(const eastl::string& source, const eastl::string& file, const eastl::string& entry_point,
    GfxShaderType type, const eastl::vector<eastl::string>& defines, GfxShaderCompilerFlags flags,
    eastl::vector<uint8_t>& output_blob)
{
    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = source.data();
    sourceBuffer.Size = source.length();
    sourceBuffer.Encoding = DXC_CP_ACP;

    eastl::wstring wstrFile = string_to_wstring(file);
    eastl::wstring wstrEntryPoint = string_to_wstring(entry_point);
    eastl::wstring wstrProfile = GetShaderProfile(type);

    eastl::vector<eastl::wstring> wstrDefines;
    for (size_t i = 0; i < defines.size(); ++i)
    {
        wstrDefines.push_back(string_to_wstring(defines[i]));
    }

    eastl::vector<LPCWSTR> arguments;
    arguments.push_back(wstrFile.c_str());
    arguments.push_back(L"-E"); arguments.push_back(wstrEntryPoint.c_str());
    arguments.push_back(L"-T"); arguments.push_back(wstrProfile.c_str());
    for (size_t i = 0; i < wstrDefines.size(); ++i)
    {
        arguments.push_back(L"-D"); arguments.push_back(wstrDefines[i].c_str());
    }

    switch (m_pRenderer->GetDevice()->GetVendor())
    {
    case GfxVendor::AMD:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_VENDOR_AMD=1");
        break;
    case GfxVendor::Intel:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_VENDOR_INTEL=1");
        break;
    case GfxVendor::Nvidia:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_VENDOR_NV=1");
        break;
    case GfxVendor::Apple:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_VENDOR_APPLE=1");
        break;
    default:
        break;
    }

    switch (m_pRenderer->GetDevice()->GetDesc().backend)
    {
    case GfxRenderBackend::D3D12:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_BACKEND_D3D12=1");
        break;
    case GfxRenderBackend::Vulkan:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_BACKEND_VULKAN=1");
        arguments.push_back(L"-spirv");
        arguments.push_back(L"-fspv-target-env=vulkan1.3");
        arguments.push_back(L"-fvk-use-dx-layout");
        arguments.push_back(L"-fvk-bind-resource-heap");
        arguments.push_back(L"0");
        arguments.push_back(L"1");
        arguments.push_back(L"-fvk-bind-sampler-heap");
        arguments.push_back(L"0");
        arguments.push_back(L"2");
        break;
    case GfxRenderBackend::Metal:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_BACKEND_METAL=1");
        break;
    case GfxRenderBackend::Mock:
        arguments.push_back(L"-D");
        arguments.push_back(L"GFX_BACKEND_MOCK=1");
        break;
    default:
        break;
    }

    arguments.push_back(L"-HV 2021");
    arguments.push_back(L"-enable-16bit-types");

#ifdef _DEBUG
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
#endif

    if (flags & GfxShaderCompilerFlagO3)
    {
        arguments.push_back(L"-O3");
    }
    else if (flags & GfxShaderCompilerFlagO2)
    {
        arguments.push_back(L"-O2");
    }
    else if (flags & GfxShaderCompilerFlagO1)
    {
        arguments.push_back(L"-O1");
    }
    else if (flags & GfxShaderCompilerFlagO0)
    {
        arguments.push_back(L"-O0");
    }
    else
    {
#ifdef _DEBUG
        arguments.push_back(L"-O0");
#else
        arguments.push_back(L"-O3");
#endif
    }
    
#if !RE_PLATFORM_WINDOWS
    arguments.push_back(L"-Vd"); //disable dxil validation because we don't have a libdxil.so for mac
#endif

    CComPtr<IDxcResult> pResults;
    m_pDxcCompiler->Compile(&sourceBuffer, arguments.data(), (UINT32)arguments.size(), m_pDxcIncludeHandler, IID_PPV_ARGS(&pResults));

    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        RE_ERROR(pErrors->GetStringPointer());
    }

    HRESULT hr;
    pResults->GetStatus(&hr);
    if (FAILED(hr))
    {
        RE_ERROR("[ShaderCompiler] failed to compile shader : {}, {}", file, entry_point);
        return false;
    }

    CComPtr<IDxcBlob> pShader = nullptr;
    if (FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr)))
    {
        return false;
    }

#if RE_PLATFORM_MAC
    if(m_pRenderer->GetDevice()->GetDesc().backend == GfxRenderBackend::Metal)
    {
        return CompileMetalIR(file, entry_point, type, pShader->GetBufferPointer(), (uint32_t)pShader->GetBufferSize(), output_blob);
    }
    else
#else
    {
        output_blob.resize(pShader->GetBufferSize());
        memcpy(output_blob.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());
    }
#endif
    
    return true;
}
