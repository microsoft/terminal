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

// Define and then undefine WIN32_NO_STATUS because windows.h has no guard to prevent it from double defing certain statuses
// when included with ntstatus.h
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

#include <strsafe.h>

#include "../host/conddkrefs.h"

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <winconp.h>
