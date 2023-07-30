#pragma once

#include "string.h"
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

inline int ExecuteCommand(const char* cmd)
{
#if 1
    return system(cmd);
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    eastl::wstring wcmd = string_to_wstring(cmd);

    // Start the child process. 
    if (!CreateProcess(NULL,   // No module name (use command line)
        (LPWSTR)wcmd.c_str(),
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NO_WINDOW,
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)            // Pointer to PROCESS_INFORMATION structure
        )
    {
        return false;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exit_code;
#endif
}

inline void SetCurrentThreadName(const eastl::string& name)
{
    SetThreadDescription(GetCurrentThread(), string_to_wstring(name).c_str());
}