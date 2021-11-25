#pragma once

#include "d3d12_header.h"

namespace pix
{
    void Init();

    void BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event);
    void EndEvent(ID3D12GraphicsCommandList* pCommandList);
};