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