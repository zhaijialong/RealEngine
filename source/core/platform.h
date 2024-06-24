#pragma once

#ifdef _WIN32
    #define RE_PLATFORM_WINDOWS 1
#endif

#ifdef __APPLE__
    #include <TargetConditionals.h>
    
    #if TARGET_OS_MAC
        #define RE_PLATFORM_MAC 1
    #elif TARGET_OS_IPHONE
        #define RE_PLATFORM_IOS 1
    #endif
#endif
