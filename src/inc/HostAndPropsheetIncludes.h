// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

// clang-format off

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // If this is not defined, windows.h includes commdlg.h which defines FindText globally and conflicts with UIAutomation ITextRangeProvider.
#endif

// Define and then undefine WIN32_NO_STATUS because windows.h has no guard to prevent it from double defing certain statuses
// when included with ntstatus.h
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable:4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

#include <initguid.h>

#ifdef EXTERNAL_BUILD
#include <ShlObj.h>
#else
#include <shlobj_core.h>
#endif

#include <winuser.h>

#define VkKeyScanW DO_NOT_USE_VkKeyScanW_USE_OneCoreSafeVkKeyScanW
#define MapVirtualKeyW DO_NOT_USE_MapVirtualKeyW_USE_OneCoreSafeMapVirtualKeyW
#define GetKeyState DO_NOT_USE_GetKeyState_USE_OneCoreSafeGetKeyState

// This header contains some overrides for win32 APIs
// that cannot exist on OneCore
#include "../interactivity/inc/VtApiRedirection.hpp"

#include <cwchar>

// Only remaining item from w32gdip.h. There's probably a better way to do this as well.
#define IS_ANY_DBCS_CHARSET( CharSet )                              \
                   ( ((CharSet) == SHIFTJIS_CHARSET)    ? TRUE :    \
                     ((CharSet) == HANGEUL_CHARSET)     ? TRUE :    \
                     ((CharSet) == CHINESEBIG5_CHARSET) ? TRUE :    \
                     ((CharSet) == GB2312_CHARSET)      ? TRUE : FALSE )

#include "conddkrefs.h"
#include "conwinuserrefs.h"

// clang-format on
