#pragma once

#include "gfx/gfx.h"
#include "utils/math.h"
#include "utils/linear_allocator.h"

#define MAX_RENDER_BATCH_CB_COUNT (5)

struct RenderBatch
{
    RenderBatch(LinearAllocator& cb_allocator) : m_allocator(cb_allocator)
    {
    }

    const char* label = "";
    IGfxPipelineState* pso = nullptr;

    struct
    {
        void* data = nullptr;
        uint32_t data_size = 0;
    } cb[MAX_RENDER_BATCH_CB_COUNT];

    union
    {
        struct
        {
            IGfxBuffer* ib;
            GfxFormat ib_format;
            uint32_t ib_offset;
            uint32_t index_count;
        };

        struct
        {
            uint32_t dispatch_x;
            uint32_t dispatch_y;
            uint32_t dispatch_z;
        };
    };

    float3 center; //world space
    float radius = 0.0f;
    uint32_t meshletCount = 0;
    uint32_t instanceIndex = 0;

    void SetPipelineState(IGfxPipelineState* pPSO)
    {
        pso = pPSO;
    }

    void SetConstantBuffer(uint32_t slot, const void* data, size_t data_size)
    {
        RE_ASSERT(slot < MAX_RENDER_BATCH_CB_COUNT);

        if (cb[slot].data == nullptr || cb[slot].data_size < data_size)
        {
            cb[slot].data = m_allocator.Alloc((uint32_t)data_size);
        }

        cb[slot].data_size = (uint32_t)data_size;
        memcpy(cb[slot].data, data, data_size);
    }

    void SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format)
    {
        ib = buffer;
        ib_offset = offset;
        ib_format = format;
    }

    void DrawIndexed(uint32_t count)
    {
        index_count = count;
    }

    void DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        dispatch_x = group_count_x;
        dispatch_y = group_count_y;
        dispatch_z = group_count_z;
    }

private:
    LinearAllocator& m_allocator;
};

inline void DrawBatch(IGfxCommandList* pCommandList, const RenderBatch& batch)
{
    GPU_EVENT_DEBUG(pCommandList, batch.label);

    pCommandList->SetPipelineState(batch.pso);

    for (int i = 0; i < MAX_RENDER_BATCH_CB_COUNT; ++i)
    {
        if (batch.cb[i].data != nullptr)
        {
            pCommandList->SetGraphicsConstants(i, batch.cb[i].data, batch.cb[i].data_size);
        }
    }

    if (batch.pso->GetType() == GfxPipelineType::MeshShading)
    {
        pCommandList->DispatchMesh(batch.dispatch_x, batch.dispatch_y, batch.dispatch_z);
    }
    else
    {
        pCommandList->SetIndexBuffer(batch.ib, batch.ib_offset, batch.ib_format);
        pCommandList->DrawIndexed(batch.index_count);
    }
}