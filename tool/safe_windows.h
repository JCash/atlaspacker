// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_TOOL_SAFE_WINDOWS_H
#define ATLASPACKER_TOOL_SAFE_WINDOWS_H

#if defined(_MSC_VER)

#ifndef DMSDK_SAFE_WINDOWS_H
#define DMSDK_SAFE_WINDOWS_H

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifdef PlaySound
#undef PlaySound
#endif

#ifdef DrawText
#undef DrawText
#endif

#ifdef DispatchMessage
#undef DispatchMessage
#endif

#ifdef FreeModule
#undef FreeModule
#endif

#ifdef GetTextMetrics
#undef GetTextMetrics
#endif

#undef MAX_TOUCH_COUNT

#endif // DMSDK_SAFE_WINDOWS_H

#endif // defined(_MSC_VER)

#endif // ATLASPACKER_TOOL_SAFE_WINDOWS_H
