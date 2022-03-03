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

static inline std::string LoadFile(const std::string& path)
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

    std::string content;
    content.resize(length);

    is.read((char*)content.data(), length);
    is.close();

    return content;
}

ShaderCache::ShaderCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IGfxShader* ShaderCache::GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    std::string file_path = Engine::GetInstance()->GetShaderPath() + file;
    std::string absolute_path = std::filesystem::absolute(file_path).string();

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
        m_cachedShaders.insert(std::make_pair(desc, pShader));
    }

    return pShader;
}

std::string ShaderCache::GetCachedFileContent(const std::string& file)
{
    auto iter = m_cachedFile.find(file);
    if (iter != m_cachedFile.end())
    {
        return iter->second;
    }

    std::string source = LoadFile(file);

    m_cachedFile.insert(std::make_pair(file, source));

    return source;
}

void ShaderCache::ReloadShaders()
{
    for (auto iter = m_cachedFile.begin(); iter != m_cachedFile.end(); ++iter)
    {
        const std::string& path = iter->first;
        const std::string& source = iter->second;
        
        std::string new_source = LoadFile(path);

        if (source != new_source)
        {
            m_cachedFile[path] = new_source;

            std::vector<IGfxShader*> changedShaders = GetShaderList(path);
            for (size_t i = 0; i < changedShaders.size(); ++i)
            {
                RecompileShader(changedShaders[i]);
            }
        }
    }
}

IGfxShader* ShaderCache::CreateShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines)
{
    std::string source = GetCachedFileContent(file);

    std::vector<uint8_t> shader_blob;
    if (!m_pRenderer->GetShaderCompiler()->Compile(source, file, entry_point, profile, defines, shader_blob))
    {
        return nullptr;
    }

    GfxShaderDesc desc;
    desc.file = file;
    desc.entry_point = entry_point;
    desc.profile = profile;
    desc.defines = defines;

    std::string name = file + " : " + entry_point + "(" + profile + ")";
    IGfxShader* shader = m_pRenderer->GetDevice()->CreateShader(desc, shader_blob, name);
    return shader;
}

void ShaderCache::RecompileShader(IGfxShader* shader)
{
    const GfxShaderDesc& desc = shader->GetDesc();
    std::string source = GetCachedFileContent(desc.file);

    std::vector<uint8_t> shader_blob;
    if (!m_pRenderer->GetShaderCompiler()->Compile(source, desc.file, desc.entry_point, desc.profile, desc.defines, shader_blob))
    {
        return;
    }

    shader->SetShaderData(shader_blob.data(), (uint32_t)shader_blob.size());

    PipelineStateCache* pipelineCache = m_pRenderer->GetPipelineStateCache();
    pipelineCache->RecreatePSO(shader);
}

std::vector<IGfxShader*> ShaderCache::GetShaderList(const std::string& file)
{
    std::vector<IGfxShader*> shaders;

    for (auto iter = m_cachedShaders.begin(); iter != m_cachedShaders.end(); ++iter)
    {
        if (IsFileIncluded(iter->second.get(), file))
        {
            shaders.push_back(iter->second.get());
        }
    }

    return shaders;
}

bool ShaderCache::IsFileIncluded(const IGfxShader* shader, const std::string& file)
{
    const GfxShaderDesc& desc = shader->GetDesc();

    if (desc.file == file)
    {
        return true;
    }

    std::string extension = std::filesystem::path(file).extension().string();
    bool is_header = extension == ".hlsli" || extension == ".h";

    if (is_header)
    {
        std::string source = GetCachedFileContent(desc.file);
        std::regex r("#include\\s*\"\\s*\\S+.\\S+\\s*\"");

        std::smatch result;
        if (std::regex_search(source, result, r))
        {
            for (size_t i = 0; i < result.size(); ++i)
            {
                std::string include = result[i].str();

                size_t first = include.find_first_of('\"');
                size_t last = include.find_last_of('\"');
                std::string header = include.substr(first + 1, last - first - 1);

                std::filesystem::path path(desc.file);
                std::filesystem::path header_path = path.parent_path() / header;

                if (std::filesystem::absolute(header_path).string() == file)
                {
                    return true;
                }
            }
        }
    }

    return false;
}
