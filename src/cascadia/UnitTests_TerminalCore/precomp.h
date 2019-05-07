/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).

Author(s):
- Carlos Zamora (cazamor) April 2019
--*/

#pragma once

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#ifdef BUILDING_INSIDE_WINIDE
#define DbgRaiseAssertionFailure() __int2c()
#endif

#include <ShellScalingApi.h>

// Comment to build against the private SDK.
#define CON_BUILD_PUBLIC

#ifdef CON_BUILD_PUBLIC
#define CON_USERPRIVAPI_INDIRECT
#define CON_DPIAPI_INDIRECT
#endif