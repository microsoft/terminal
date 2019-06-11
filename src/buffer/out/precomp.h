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

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#pragma warning(push)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

// Windows Header Files:
#include <windows.h>
#include <intsafe.h>

// private dependencies
#include "..\inc\operators.hpp"
#include "..\inc\unicode.hpp"
#pragma warning(pop)

// clang-format on
