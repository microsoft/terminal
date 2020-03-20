/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

#pragma once

// Ignore checked iterators warning from VC compiler.
#define _SCL_SECURE_NO_WARNINGS

// Block minwindef.h min/max macros to prevent <algorithm> conflict
#define NOMINMAX

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#define INLINE_NTSTATUS_FROM_WIN32 1 // Must use inline NTSTATUS or it will call the wrapped function twice.
#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include "../host/conddkrefs.h"
#include <winuser.h>
#include <winconp.h>
#include <ntcon.h>
#include <windowsx.h>
#include <dde.h>

// TODO: MSFT 9355094 Find a better way of doing this. http://osgvsowi/9355094
[[nodiscard]] inline NTSTATUS NTSTATUS_FROM_HRESULT(HRESULT hr)
{
    return NTSTATUS_FROM_WIN32(HRESULT_CODE(hr));
}
