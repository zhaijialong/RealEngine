#pragma once

#include "IntelTAA.h"

#ifdef __cplusplus
using FTAAResolve = IntelTAA::FTAAResolve;
#endif

struct TAAConstant
{
    uint velocityRT;
    uint colorRT;
    uint historyRT;
    uint depthRT;
    
    uint prevDepthRT;
    uint outTexture;
    uint _padding0;
    uint _padding1;
    
    FTAAResolve consts;
};

#ifndef __cplusplus
ConstantBuffer<TAAConstant> taaCB : register(b1);
#endif