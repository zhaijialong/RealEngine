#pragma once

#include "../gfx/gfx.h"
#include "EASTL/hash_map.h"
#include "EASTL/unique_ptr.h"

namespace eastl
{
    template <>
    struct hash<GfxShaderDesc>
    {
        size_t operator()(const GfxShaderDesc& desc) const
        {
            eastl::string s = desc.file + desc.entry_point + desc.profile;
            for (size_t i = 0; i < desc.defines.size(); ++i)
            {
                s += desc.defines[i];
            }

            return eastl::hash<eastl::string>{}(s);
        }
    };
}

class Renderer;

class ShaderCache
{
public:
    ShaderCache(Renderer* pRenderer);

    IGfxShader* GetShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines);
    eastl::string GetCachedFileContent(const eastl::string& file);

    void ReloadShaders();

private:
    IGfxShader* CreateShader(const eastl::string& file, const eastl::string& entry_point, const eastl::string& profile, const eastl::vector<eastl::string>& defines);
    void RecompileShader(IGfxShader* shader);

    eastl::vector<IGfxShader*> GetShaderList(const eastl::string& file);
    bool IsFileIncluded(const IGfxShader* shader, const eastl::string& file);

private:
    Renderer* m_pRenderer;
    eastl::hash_map<GfxShaderDesc, eastl::unique_ptr<IGfxShader>> m_cachedShaders;
    eastl::hash_map<eastl::string, eastl::string> m_cachedFile;
};