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

inline bool operator==(const GfxMeshShadingPipelineDesc& lhs, const GfxMeshShadingPipelineDesc& rhs)
{
    if (lhs.ms->GetHash() != rhs.ms->GetHash())
    {
        return false;
    }

    uint64_t lhs_ps_hash = lhs.ps ? lhs.ps->GetHash() : 0;
    uint64_t rhs_ps_hash = rhs.ps ? rhs.ps->GetHash() : 0;
    if (lhs_ps_hash != rhs_ps_hash)
    {
        return false;
    }

    uint64_t lhs_as_hash = lhs.as ? lhs.as->GetHash() : 0;
    uint64_t rhs_as_hash = rhs.as ? rhs.as->GetHash() : 0;
    if (lhs_as_hash != rhs_as_hash)
    {
        return false;
    }

    const size_t state_offset = offsetof(GfxMeshShadingPipelineDesc, rasterizer_state);
    void* lhs_states = (char*)&lhs + state_offset;
    void* rhs_states = (char*)&rhs + state_offset;

    return memcmp(lhs_states, rhs_states, sizeof(GfxMeshShadingPipelineDesc) - state_offset) == 0;
}

inline bool operator==(const GfxComputePipelineDesc& lhs, const GfxComputePipelineDesc& rhs)
{
    return lhs.cs->GetHash() == rhs.cs->GetHash();
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
        return iter->second.get();
    }

    IGfxPipelineState* pPSO = m_pRenderer->GetDevice()->CreateGraphicsPipelineState(desc, name);
    if (pPSO)
    {
        m_cachedGraphicsPSO.insert(std::make_pair(desc, pPSO));
    }

    return pPSO;
}

IGfxPipelineState* PipelineStateCache::GetPipelineState(const GfxMeshShadingPipelineDesc& desc, const std::string& name)
{
    auto iter = m_cachedMeshShadingPSO.find(desc);
    if (iter != m_cachedMeshShadingPSO.end())
    {
        return iter->second.get();
    }

    IGfxPipelineState* pPSO = m_pRenderer->GetDevice()->CreateMeshShadingPipelineState(desc, name);
    if (pPSO)
    {
        m_cachedMeshShadingPSO.insert(std::make_pair(desc, pPSO));
    }

    return pPSO;
}

IGfxPipelineState* PipelineStateCache::GetPipelineState(const GfxComputePipelineDesc& desc, const std::string& name)
{
    auto iter = m_cachedComputePSO.find(desc);
    if (iter != m_cachedComputePSO.end())
    {
        return iter->second.get();
    }

    IGfxPipelineState* pPSO = m_pRenderer->GetDevice()->CreateComputePipelineState(desc, name);
    if (pPSO)
    {
        m_cachedComputePSO.insert(std::make_pair(desc, pPSO));
    }

    return pPSO;
}

void PipelineStateCache::RecreatePSO(IGfxShader* shader)
{
    for (auto iter = m_cachedGraphicsPSO.begin(); iter != m_cachedGraphicsPSO.end(); ++iter)
    {
        const GfxGraphicsPipelineDesc& desc = iter->first;
        IGfxPipelineState* pso = iter->second.get();

        if (desc.vs == shader ||
            desc.ps == shader)
        {
            pso->Create();
        }
    }

    for (auto iter = m_cachedMeshShadingPSO.begin(); iter != m_cachedMeshShadingPSO.end(); ++iter)
    {
        const GfxMeshShadingPipelineDesc& desc = iter->first;
        IGfxPipelineState* pso = iter->second.get();

        if (desc.as == shader ||
            desc.ms == shader ||
            desc.ps == shader)
        {
            pso->Create();
        }
    }

    for (auto iter = m_cachedComputePSO.begin(); iter != m_cachedComputePSO.end(); ++iter)
    {
        const GfxComputePipelineDesc& desc = iter->first;
        IGfxPipelineState* pso = iter->second.get();

        if (desc.cs == shader)
        {
            pso->Create();
        }
    }
}
