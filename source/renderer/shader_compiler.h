#pragma once

#include <string>
#include <vector>

struct IDxcCompiler3;
struct IDxcUtils;
struct IDxcIncludeHandler;

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    bool Compile(const std::string& source, const std::string& file, const std::string& entry_point, 
        const std::string& profile, const std::vector<std::string>& defines,
        std::vector<uint8_t>& output_blob);

private:
    IDxcCompiler3* m_pDxcCompiler = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    IDxcIncludeHandler* m_pDxcIncludeHandler = nullptr;
};