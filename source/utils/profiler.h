#pragma once

#include "microprofile/microprofile.h"

#define CPU_EVENT(name, color) MICROPROFILE_SCOPEI("RealEngine", name, color)

inline void StartProfiler()
{
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);

    MicroProfileOnThreadCreate("Main");
}

inline void ShutdownProfiler()
{
    MicroProfileShutdown();
}

inline void TickProfiler()
{
    MicroProfileFlip(0);
}