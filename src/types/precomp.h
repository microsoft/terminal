/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// clang-format off

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows Header Files:
#include <windows.h>
#include <objbase.h>
#include <bcrypt.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

typedef long NTSTATUS;
#define NT_SUCCESS(Status)  (((NTSTATUS)(Status)) >= 0)
#define STATUS_SHARING_VIOLATION         ((NTSTATUS)0xC0000043L)
#define STATUS_INSUFFICIENT_RESOURCES ((DWORD)0xC000009AL)
#define STATUS_ILLEGAL_FUNCTION ((DWORD)0xC00000AFL)
#define STATUS_PIPE_DISCONNECTED ((DWORD)0xC00000B0L)
#define STATUS_BUFFER_TOO_SMALL ((DWORD)0xC0000023L)

//
// Map a WIN32 error value into an NTSTATUS
// Note: This assumes that WIN32 errors fall in the range -32k to 32k.
//

#define FACILITY_NTWIN32                 0x7

#define __NTSTATUS_FROM_WIN32(x) ((NTSTATUS)(x) <= 0 ? ((NTSTATUS)(x)) : ((NTSTATUS) (((x) & 0x0000FFFF) | (FACILITY_NTWIN32 << 16) | ERROR_SEVERITY_ERROR)))

#ifdef INLINE_NTSTATUS_FROM_WIN32
#ifndef __midl
__inline NTSTATUS_FROM_WIN32(long x) { return x <= 0 ? (NTSTATUS)x : (NTSTATUS)(((x) & 0x0000FFFF) | (FACILITY_NTWIN32 << 16) | ERROR_SEVERITY_ERROR); }
#else
#define NTSTATUS_FROM_WIN32(x) __NTSTATUS_FROM_WIN32(x)
#endif
#else
#define NTSTATUS_FROM_WIN32(x) __NTSTATUS_FROM_WIN32(x)
#endif

#include <winioctl.h>
#pragma prefast(push)
#pragma prefast(disable:26071, "Range violation in Intsafe. Not ours.")
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS // Only unsigned intsafe math/casts available without this def
#include <intsafe.h>
#pragma prefast(pop)

// private dependencies
#pragma warning(push)
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include "..\host\conddkrefs.h"
#pragma warning(pop)

#include <conmsgl1.h>
#include <conmsgl2.h>
#include <conmsgl3.h>
#include <condrv.h>
#include <ntcon.h>

// TODO: MSFT 9355094 Find a better way of doing this. http://osgvsowi/9355094
[[nodiscard]]
constexpr NTSTATUS NTSTATUS_FROM_HRESULT(HRESULT hr) noexcept
{
    return NTSTATUS_FROM_WIN32(HRESULT_CODE(hr));
}

// clang-format on
