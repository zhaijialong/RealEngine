#pragma once

#include <stdint.h>
#include <Windows.h>

inline uint32_t GetWindowWidth(void* window)
{
    RECT rect;
    GetClientRect((HWND)window, &rect);
    return rect.right - rect.left;
}

inline uint32_t GetWindowHeight(void* window)
{
    RECT rect;
    GetClientRect((HWND)window, &rect);
    return rect.bottom - rect.top;
}
