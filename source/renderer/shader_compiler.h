#pragma once

#include "EASTL/vector.h"
#include "EASTL/string.h"

struct IDxcCompiler3;
struct IDxcUtils;
struct IDxcIncludeHandler;

class Renderer;

class ShaderCompiler
{
public:
    ShaderCompiler(Renderer* pRenderer);
    ~ShaderCompiler();

    bool Compile(const eastl::string& source, const eastl::string& file, const eastl::string& entry_point, 
        const eastl::string& profile, const eastl::vector<eastl::string>& defines,
        eastl::vector<uint8_t>& output_blob);

private:
    Renderer* m_pRenderer = nullptr;
    IDxcCompiler3* m_pDxcCompiler = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    IDxcIncludeHandler* m_pDxcIncludeHandler = nullptr;
};