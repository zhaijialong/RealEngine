#pragma once

#include "xxHash/xxhash.h"
#include "microprofile/microprofile.h"

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

class CpuEvent
{
public:
    CpuEvent(const char* group, const char* name)
    {
#if MICROPROFILE_ENABLED
        static const uint32_t EVENT_COLOR[] =
        {
            MP_LIGHTCYAN4,
            MP_SKYBLUE2,
            MP_SEAGREEN4,
            MP_LIGHTGOLDENROD4,
            MP_BROWN3,
            MP_MEDIUMPURPLE2,
            MP_SIENNA,
            MP_LIMEGREEN,
            MP_MISTYROSE,
            MP_LIGHTYELLOW,
        };

        uint32_t color_count = sizeof(EVENT_COLOR) / sizeof(EVENT_COLOR[0]);
        uint32_t color = EVENT_COLOR[XXH32(name, strlen(name), 0) % color_count];

        MicroProfileToken token = MicroProfileGetToken(group, name, color, MicroProfileTokenTypeCpu);
        MicroProfileEnter(token);
#endif
    }

    ~CpuEvent()
    {
#if MICROPROFILE_ENABLED
        MicroProfileLeave();
#endif
    }
};

#define CPU_EVENT(group, name) CpuEvent __cpu_event__(group, name)
