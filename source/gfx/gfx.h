#pragma once

#include "gfx_define.h"
#include "i_gfx_device.h"
#include "i_gfx_buffer.h"
#include "i_gfx_texture.h"
#include "i_gfx_command_list.h"
#include "i_gfx_fence.h"
#include "i_gfx_shader.h"
#include "i_gfx_pipeline_state.h"
#include "i_gfx_swapchain.h"

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc);

class RenderEvent
{
public:
    RenderEvent(IGfxCommandList* pCommandList, const std::string& event_name) :
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

#define RENDER_EVENT(pCommandList, event_name) RenderEvent __render_event__(pCommandList, event_name)