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

private:
    IGfxShader* CreateShader(const std::string& file, const std::string& entry_point, const std::string& profile, const std::vector<std::string>& defines);

private:
    Renderer* m_pRenderer;
    std::unordered_map<GfxShaderDesc, std::unique_ptr<IGfxShader>> m_cachedShaders;
};