#include "shader_compiler.h"
#include "renderer.h"
#include "shader_cache.h"
#include "utils/log.h"
#include "utils/string.h"
#include "utils/assert.h"
#include "dxc/dxcapi.h"

#include <filesystem>
#include <atlbase.h> //CComPtr

class DXCIncludeHandler : public IDxcIncludeHandler
{
public:
    DXCIncludeHandler(ShaderCache* pShaderCache, IDxcUtils* pDxcUtils) : m_pShaderCache(pShaderCache), m_pDxcUtils(pDxcUtils)
    {
    }

    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override
    {
        std::string absolute_path = std::filesystem::absolute(fileName).string();
        std::string source = m_pShaderCache->GetCachedFileContent(absolute_path);

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
    HMODULE dxc = LoadLibrary(L"dxcompiler.dll");
    if (dxc)
    {
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc, "DxcCreateInstance");

        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_pDxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_pDxcCompiler));

        //m_pDxcUtils->CreateDefaultIncludeHandler(&m_pDxcIncludeHandler);
        m_pDxcIncludeHandler = new DXCIncludeHandler(pRenderer->GetShaderCache(), m_pDxcUtils);
        m_pDxcIncludeHandler->AddRef();
    }
}

ShaderCompiler::~ShaderCompiler()
{
    m_pDxcIncludeHandler->Release();
    m_pDxcCompiler->Release();
    m_pDxcUtils->Release();
}

bool ShaderCompiler::Compile(const std::string& source, const std::string& file, const std::string& entry_point,
    const std::string& profile, const std::vector<std::string>& defines,
    std::vector<uint8_t>& output_blob)
{
    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = source.data();
    sourceBuffer.Size = source.length();
    sourceBuffer.Encoding = DXC_CP_ACP;

    std::wstring wstrFile = string_to_wstring(file);
    std::wstring wstrEntryPoint = string_to_wstring(entry_point);
    std::wstring wstrProfile = string_to_wstring(profile);

    std::vector<std::wstring> wstrDefines;
    for (size_t i = 0; i < defines.size(); ++i)
    {
        wstrDefines.push_back(string_to_wstring(defines[i]));
    }

    std::vector<LPCWSTR> arguments;
    arguments.push_back(wstrFile.c_str());
    arguments.push_back(L"-E"); arguments.push_back(wstrEntryPoint.c_str());
    arguments.push_back(L"-T"); arguments.push_back(wstrProfile.c_str());
    for (size_t i = 0; i < wstrDefines.size(); ++i)
    {
        arguments.push_back(L"-D"); arguments.push_back(wstrDefines[i].c_str());
    }

    arguments.push_back(L"-D");
    switch (m_pRenderer->GetDevice()->GetVendor())
    {
    case GfxVendor::AMD:
        arguments.push_back(L"GFX_VENDOR_AMD=1");
        break;
    case GfxVendor::Intel:
        arguments.push_back(L"GFX_VENDOR_INTEL=1");
        break;
    case GfxVendor::Nvidia:
        arguments.push_back(L"GFX_VENDOR_NV=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    arguments.push_back(L"-HV 2021");
    arguments.push_back(L"-enable-16bit-types");

#ifdef _DEBUG
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-O0");
#endif

    CComPtr<IDxcResult> pResults;
    m_pDxcCompiler->Compile(&sourceBuffer, arguments.data(), (UINT32)arguments.size(), m_pDxcIncludeHandler, IID_PPV_ARGS(&pResults));

    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        RE_LOG(pErrors->GetStringPointer());
    }

    HRESULT hr;
    pResults->GetStatus(&hr);
    if (FAILED(hr))
    {
        return false;
    }

    CComPtr<IDxcBlob> pShader = nullptr;
    if (FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr)))
    {
        return false;
    }

    output_blob.resize(pShader->GetBufferSize());
    memcpy(output_blob.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());

    return true;
}
