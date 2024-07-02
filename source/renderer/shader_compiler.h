#pragma once

#include "core/platform.h"
#include "gfx/gfx_defines.h"

struct IDxcCompiler3;
struct IDxcUtils;
struct IDxcIncludeHandler;
struct IRCompiler;
struct IRRootSignature;

class Renderer;

class ShaderCompiler
{
public:
    ShaderCompiler(Renderer* pRenderer);
    ~ShaderCompiler();

    bool Compile(const eastl::string& source, const eastl::string& file, const eastl::string& entry_point, 
        GfxShaderType type, const eastl::vector<eastl::string>& defines, GfxShaderCompilerFlags flags,
        eastl::vector<uint8_t>& output_blob);

#if RE_PLATFORM_MAC
private:
    void CreateMetalCompiler();
    void DestroyMetalCompiler();
    bool CompileMetalIR(const eastl::string& file, const eastl::string& entry_point, GfxShaderType type,
        const void* data, uint32_t data_size, eastl::vector<uint8_t>& output_blob);
#endif
    
private:
    Renderer* m_pRenderer = nullptr;
    IDxcCompiler3* m_pDxcCompiler = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    IDxcIncludeHandler* m_pDxcIncludeHandler = nullptr;
    
#if RE_PLATFORM_MAC
    IRCompiler* m_pMetalCompiler = nullptr;
    IRRootSignature* m_pMetalRootSignature = nullptr;
#endif
};
