#pragma once

#include "../gfx/gfx.h"
#include "xxHash/xxhash.h"
#include <unordered_map>
#include <memory>

//cityhash Hash128to64
inline uint64_t hash_combine_64(uint64_t hash0, uint64_t hash1)
{
    const uint64_t kMul = 0x9ddfea08eb382d69ULL;
    uint64_t a = (hash1 ^ hash0) * kMul;
    a ^= (a >> 47);
    uint64_t b = (hash0 ^ a) * kMul;
    b ^= (b >> 47);
    return b * kMul;
}

namespace std
{
    template <>
    struct hash<GfxGraphicsPipelineDesc>
    {
        size_t operator()(const GfxGraphicsPipelineDesc& desc) const
        {
            uint64_t vs_hash = desc.vs->GetHash();
            uint64_t ps_hash = desc.ps ? desc.ps->GetHash() : 0;

            const size_t state_offset = offsetof(GfxGraphicsPipelineDesc, rasterizer_state);
            uint64_t state_hash = XXH3_64bits((char*)&desc + state_offset, sizeof(GfxGraphicsPipelineDesc) - state_offset);

            uint64_t hash = hash_combine_64(hash_combine_64(vs_hash, ps_hash), state_hash);
            
            static_assert(sizeof(size_t) == sizeof(uint64_t), "only supports 64 bits platforms");
            return hash;
        }
    };
}

class Renderer;

class PipelineStateCache
{
public:
    PipelineStateCache(Renderer* pRenderer);

    IGfxPipelineState* GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name);

private:
    Renderer* m_pRenderer;
    std::unordered_map<GfxGraphicsPipelineDesc, std::unique_ptr<IGfxPipelineState>> m_cachedGraphicsPSO;
};