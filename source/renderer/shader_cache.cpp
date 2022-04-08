#include "shader_cache.h"
#include "renderer.h"
#include "shader_compiler.h"
#include "pipeline_cache.h"
#include "core/engine.h"
#include <fstream>
#include <filesystem>
#include <regex>

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

static inline eastl::string LoadFile(const eastl::string& path)
{
    std::ifstream is;
    is.open(path.c_str(), std::ios::binary);
    if (is.fail())
    {
        return "";
    }

    is.seekg(0, std::ios::end);
    uint32_t length = (uint32_t)is.tellg();
    is.seekg(0, std::ios::beg);

    eastl::string content;
    content.resize(length);

    is.read((char*)content.data(), length);
    is.close();

    return content;
}

ShaderCache::ShaderCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IGfxShader* ShaderCache::GetShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines)
{
    eastl::string file_path = Engine::GetInstance()->GetShaderPath() + file;
    eastl::string absolute_path = std::filesystem::absolute(file_path.c_str()).string().c_str();

    GfxShaderDesc desc;
    desc.file = absolute_path;
    desc.entry_point = entry_point;
    desc.profile = profile;
    desc.defines = defines;

    auto iter = m_cachedShaders.find(desc);
    if (iter != m_cachedShaders.end())
    {
        return iter->second.get();
    }

    IGfxShader* pShader = CreateShader(absolute_path, entry_point, profile, defines);
    if (pShader != nullptr)
    {
        m_cachedShaders.insert(eastl::make_pair(desc, eastl::unique_ptr<IGfxShader>(pShader)));
    }

    return pShader;
}

eastl::string ShaderCache::GetCachedFileContent(const eastl::string& file)
{
    auto iter = m_cachedFile.find(file);
    if (iter != m_cachedFile.end())
    {
        return iter->second;
    }

    eastl::string source = LoadFile(file);

    m_cachedFile.insert(eastl::make_pair(file, source));

    return source;
}

void ShaderCache::ReloadShaders()
{
    for (auto iter = m_cachedFile.begin(); iter != m_cachedFile.end(); ++iter)
    {
        const eastl::string& path = iter->first;
        const eastl::string& source = iter->second;
        
        eastl::string new_source = LoadFile(path);

        if (source != new_source)
        {
            m_cachedFile[path] = new_source;

            eastl::vector<IGfxShader*> changedShaders = GetShaderList(path);
            for (size_t i = 0; i < changedShaders.size(); ++i)
            {
                RecompileShader(changedShaders[i]);
            }
        }
    }
}

IGfxShader* ShaderCache::CreateShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines)
{
    eastl::string source = GetCachedFileContent(file);

    eastl::vector<uint8_t> shader_blob;
    if (!m_pRenderer->GetShaderCompiler()->Compile(source, file, entry_point, profile, defines, shader_blob))
    {
        return nullptr;
    }

    GfxShaderDesc desc;
    desc.file = file;
    desc.entry_point = entry_point;
    desc.profile = profile;
    desc.defines = defines;

    eastl::string name = file + " : " + entry_point + "(" + profile + ")";
    IGfxShader* shader = m_pRenderer->GetDevice()->CreateShader(desc, shader_blob, name);
    return shader;
}

void ShaderCache::RecompileShader(IGfxShader* shader)
{
    const GfxShaderDesc& desc = shader->GetDesc();
    eastl::string source = GetCachedFileContent(desc.file);

    eastl::vector<uint8_t> shader_blob;
    if (!m_pRenderer->GetShaderCompiler()->Compile(source, desc.file, desc.entry_point, desc.profile, desc.defines, shader_blob))
    {
        return;
    }

    shader->SetShaderData(shader_blob.data(), (uint32_t)shader_blob.size());

    PipelineStateCache* pipelineCache = m_pRenderer->GetPipelineStateCache();
    pipelineCache->RecreatePSO(shader);
}

eastl::vector<IGfxShader*> ShaderCache::GetShaderList(const eastl::string& file)
{
    eastl::vector<IGfxShader*> shaders;

    for (auto iter = m_cachedShaders.begin(); iter != m_cachedShaders.end(); ++iter)
    {
        if (IsFileIncluded(iter->second.get(), file))
        {
            shaders.push_back(iter->second.get());
        }
    }

    return shaders;
}

bool ShaderCache::IsFileIncluded(const IGfxShader* shader, const eastl::string& file)
{
    const GfxShaderDesc& desc = shader->GetDesc();

    if (desc.file == file)
    {
        return true;
    }

    eastl::string extension = std::filesystem::path(file.c_str()).extension().string().c_str();
    bool is_header = extension == ".hlsli" || extension == ".h";

    if (is_header)
    {
        std::string source = GetCachedFileContent(desc.file).c_str();
        std::regex r("#include\\s*\"\\s*\\S+.\\S+\\s*\"");

        std::smatch result;
        while (std::regex_search(source, result, r))
        {
            eastl::string include = result[0].str().c_str();

            size_t first = include.find_first_of('\"');
            size_t last = include.find_last_of('\"');
            eastl::string header = include.substr(first + 1, last - first - 1);

            std::filesystem::path path(desc.file.c_str());
            std::filesystem::path header_path = path.parent_path() / header.c_str();

            if (std::filesystem::absolute(header_path).string().c_str() == file)
            {
                return true;
            }

            source = result.suffix();
        }
    }

    return false;
}
