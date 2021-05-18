#pragma once

#include <Windows.h>
#include <string>

inline std::wstring string_to_wstring(std::string s)
{
	DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);

	std::wstring result;
	result.resize(size);

	MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);

	return result;
}
