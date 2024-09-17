#pragma once

#include "gfx/gfx.h"

void ClearUAV(IGfxCommandList* commandList, IGfxResource* resource, IGfxDescriptor* descriptor, const GfxUnorderedAccessViewDesc& uavDesc, const float* value);

void ClearUAV(IGfxCommandList* commandList, IGfxResource* resource, IGfxDescriptor* descriptor, const GfxUnorderedAccessViewDesc& uavDesc, const uint32_t* value);