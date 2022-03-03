#pragma once

#include "../gfx/gfx.h"
#include <unordered_map>
#include <memory>

namespace std
{
    template <>
    struct hash<GfxShaderDesc>
    {
        size_t operator()(const GfxShaderDesc& desc) const
        {
            std::string s = desc.file + desc.entry_point + desc.profile;
            for (size_t i = 0; i < desc.defines.size(); ++i)
            {
                s += desc.defines[i];
            }

            return std::hash<std::string>{}(s);
        }
    };
}

class Renderer;

class ShaderCache
{
public:
    ShaderCache(Renderer* pRenderer);

    IGfxShader* GetShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    std::string GetCachedFileContent(const std::string& file);

    void ReloadShaders();

private:
    IGfxShader* CreateShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);
    void RecompileShader(IGfxShader* shader);

    std::vector<IGfxShader*> GetShaderList(const std::string& file);
    bool IsFileIncluded(const IGfxShader* shader, const std::string& file);

private:
    Renderer* m_pRenderer;
    std::unordered_map<GfxShaderDesc, std::unique_ptr<IGfxShader>> m_cachedShaders;
    std::unordered_map<std::string, std::string> m_cachedFile;
};