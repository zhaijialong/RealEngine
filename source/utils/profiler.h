#pragma once

#include "gfx/gfx.h"
#include "tracy/public/tracy/Tracy.hpp"

#define CPU_EVENT(group, name) ZoneScopedN(name)
#define GPU_EVENT(pCommandList, event_name) ScopedGpuEvent __gpu_event(pCommandList, event_name, __FILE__, __FUNCTION__, __LINE__)