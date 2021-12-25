#include "shader_cache.h"
#include "renderer.h"
#include "shader_compiler.h"
#include "core/engine.h"
#include <fstream>

inline bool operator==(const GfxShaderDesc& lhs, const GfxShaderDesc& rhs)
{
    if (lhs.file != rhs.file || lhs.entry_point != rhs.entry_point || lhs.profile != rhs.profile)
    {
        return false;
    }

    if (lhs.defines.size() != rhs.defines.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.defines.size(); ++i)
    {
        if (lhs.defines[i] != rhs.defines[i])
        {
            return false;
        }
    }

    return true;
}

ShaderCache::ShaderCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IGfxShader* ShaderCache::GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    GfxShaderDesc desc;
    desc.file = file;
    desc.entry_point = entry_point;
    desc.profile = profile;
    desc.defines = defines;

    auto iter = m_cachedShaders.find(desc);
    if (iter != m_cachedShaders.end())
    {
        return iter->second.get();
    }

    IGfxShader* pShader = CreateShader(file, entry_point, profile, defines);
    if (pShader != nullptr)
    {
        m_cachedShaders.insert(std::make_pair(desc, pShader));
    }

    return pShader;
}

IGfxShader* ShaderCache::CreateShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    std::string file_path = Engine::GetInstance()->GetShaderPath() + file;

    std::ifstream is;
    is.open(file_path.c_str(), std::ios::binary);
    if (is.fail())
    {
        return nullptr;
    }

    is.seekg(0, std::ios::end);
    uint32_t length = (uint32_t)is.tellg();
    is.seekg(0, std::ios::beg);

    std::string source;
    source.resize(length);

    is.read((char*)source.data(), length);
    is.close();

    std::vector<uint8_t> shader_blob;
    if (!m_pRenderer->GetShaderCompiler()->Compile(source, file_path, entry_point, profile, defines, shader_blob))
    {
        return nullptr;
    }

    GfxShaderDesc desc;
    desc.file = file_path;
    desc.entry_point = entry_point;
    desc.profile = profile;
    desc.defines = defines;

    std::string name = file + " : " + entry_point + "(" + profile + ")";
    IGfxShader* shader = m_pRenderer->GetDevice()->CreateShader(desc, shader_blob, name);
    return shader;
}
