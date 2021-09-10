#pragma once

#include "microprofile/microprofile.h"

#define CPU_EVENT(group, name, color) MICROPROFILE_SCOPEI(group, name, color)

inline void StartProfiler()
{
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);

    MicroProfileOnThreadCreate("CPU Main");
}

inline void ShutdownProfiler()
{
    MicroProfileShutdown();
}

inline void TickProfiler()
{
    MicroProfileFlip(0);
}