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

// clang-format off

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMCX
#define NOHELP
#define NOCOMM
#endif

// Windows Header Files:
#include <Windows.h>

#include <mmeapi.h>
#include <dsound.h>

// clang-format on
