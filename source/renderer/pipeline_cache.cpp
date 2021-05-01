#include "pipeline_cache.h"
#include "renderer.h"

inline bool operator==(const GfxGraphicsPipelineDesc& lhs, const GfxGraphicsPipelineDesc& rhs)
{
    if (lhs.vs->GetHash() != rhs.vs->GetHash())
    {
        return false;
    }

    uint64_t lhs_ps_hash = lhs.ps ? lhs.ps->GetHash() : 0;
    uint64_t rhs_ps_hash = rhs.ps ? rhs.ps->GetHash() : 0;
    if (lhs_ps_hash != rhs_ps_hash)
    {
        return false;
    }

    const size_t state_offset = offsetof(GfxGraphicsPipelineDesc, rasterizer_state);
    void* lhs_states = (char*)&lhs + state_offset;
    void* rhs_states = (char*)&rhs + state_offset;

    return memcmp(lhs_states, rhs_states, sizeof(GfxGraphicsPipelineDesc) - state_offset) == 0;
}

PipelineStateCache::PipelineStateCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IGfxPipelineState* PipelineStateCache::GetPipelineState(const GfxGraphicsPipelineDesc& desc, const std::string& name)
{
    auto iter = m_cachedGraphicsPSO.find(desc);
    if (iter != m_cachedGraphicsPSO.end())
    {
        return iter->second;
    }

    IGfxPipelineState* pPSO = m_pRenderer->GetDevice()->CreateGraphicsPipelineState(desc, name);
    if (pPSO)
    {
        m_cachedGraphicsPSO.insert(std::make_pair(desc, pPSO));
    }

    return pPSO;
}
