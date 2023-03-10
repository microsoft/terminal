/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

#include <cwchar>
#include <sal.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <Windows.h>
#include <windowsx.h>
#include <usp10.h>

#if defined(DEBUG) || defined(_DEBUG) || defined(DBG)
#define WHEN_DBG(x) x
#else
#define WHEN_DBG(x)
#endif

// SafeMath
#pragma prefast(push)
#pragma prefast(disable : 26071, "Range violation in Intsafe. Not ours.")
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS // Only unsigned intsafe math/casts available without this def
#include <intsafe.h>
#pragma prefast(pop)
