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

#include <sal.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <windows.h>
#include <wincon.h>

#include "..\..\types\inc\viewport.hpp"
#include "..\..\inc\operators.hpp"

#ifndef _NTSTATUS_DEFINED
#define _NTSTATUS_DEFINED
typedef _Return_type_success_(return >= 0) long NTSTATUS;
#endif

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
