#pragma once

#include "../render_graph.h"

class ClusteredLightLists
{
public:
    ClusteredLightLists(Renderer* pRenderer);
    ~ClusteredLightLists();

    void Build(uint32_t width, uint32_t height);
    uint32_t GetLightGridBufferAddress() const { return m_lightGridBufferAddress; }
    uint32_t GetLightIndicesBufferAddress() const { return m_lightIndicesBufferAddress; }
    uint32_t GetTileSize() const;
    uint32_t GetSliceCount() const;
    float2 GetSliceParams(class Camera* camera) const;

private:
    Renderer* m_pRenderer = nullptr;

    uint32_t m_lightGridBufferAddress = 0;
    uint32_t m_lightIndicesBufferAddress = 0;
};