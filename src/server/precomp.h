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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#endif

#define NOMINMAX

// Windows Header Files:
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

#include <winioctl.h>
#include <intsafe.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

// private dependencies
#include "../host/conddkrefs.h"

#include <conmsgl1.h>
#include <conmsgl2.h>
#include <conmsgl3.h>
#include <condrv.h>
#include <ntcon.h>

// TODO: MSFT 9355094 Find a better way of doing this. http://osgvsowi/9355094
[[nodiscard]] inline NTSTATUS NTSTATUS_FROM_HRESULT(HRESULT hr)
{
    return NTSTATUS_FROM_WIN32(HRESULT_CODE(hr));
}
