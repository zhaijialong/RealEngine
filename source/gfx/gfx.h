#pragma once

#include "gfx_defines.h"
#include "gfx_device.h"
#include "gfx_buffer.h"
#include "gfx_texture.h"
#include "gfx_command_list.h"
#include "gfx_fence.h"
#include "gfx_shader.h"
#include "gfx_pipeline_state.h"
#include "gfx_swapchain.h"
#include "gfx_descriptor.h"
#include "gfx_heap.h"
#include "gfx_rt_blas.h"
#include "gfx_rt_tlas.h"

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc);
uint32_t GetFormatRowPitch(GfxFormat format, uint32_t width);
uint32_t GetFormatBlockWidth(GfxFormat format);
uint32_t GetFormatBlockHeight(GfxFormat format);
bool IsDepthFormat(GfxFormat format);
uint32_t CalcSubresource(const GfxTextureDesc& desc, uint32_t mip, uint32_t slice);
void DecomposeSubresource(const GfxTextureDesc& desc, uint32_t subresource, uint32_t& mip, uint32_t& slice);

void BeginMPGpuEvent(IGfxCommandList* pCommandList, const eastl::string& event_name);
void EndMPGpuEvent(IGfxCommandList* pCommandList);

class RenderEvent
{
public:
    RenderEvent(IGfxCommandList* pCommandList, const eastl::string& event_name) :
        m_pCommandList(pCommandList)
    {
        pCommandList->BeginEvent(event_name);
    }

    ~RenderEvent()
    {
        m_pCommandList->EndEvent();
    }

private:
    IGfxCommandList* m_pCommandList;
};

class MPRenderEvent
{
public:
    MPRenderEvent(IGfxCommandList* pCommandList, const eastl::string& event_name) :
        m_pCommandList(pCommandList)
    {
        BeginMPGpuEvent(pCommandList, event_name);
    }

    ~MPRenderEvent()
    {
        EndMPGpuEvent(m_pCommandList);
    }

private:
    IGfxCommandList* m_pCommandList;
};

#if 1
#define GPU_EVENT(pCommandList, event_name) RenderEvent __render_event__(pCommandList, event_name); MPRenderEvent __mp_event__(pCommandList, event_name)
#define GPU_EVENT_DEBUG(pCommandList, event_name) RenderEvent __render_event__(pCommandList, event_name)
#define GPU_EVENT_PROFILER(pCommandList, event_name) MPRenderEvent __mp_event__(pCommandList, event_name)
#else
#define GPU_EVENT(pCommandList, event_name) 
#define GPU_EVENT_DEBUG(pCommandList, event_name) 
#define GPU_EVENT_PROFILER(pCommandList, event_name) 
#endif