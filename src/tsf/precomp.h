/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    precomp.h

Abstract:

    This file precompiled header file.

Author:

Revision History:

Notes:

--*/

#define NOMINMAX

#define _OLEAUT32_
#include <ole2.h>
#include <windows.h>

extern "C" {
#include <winuser.h>

#include <ime.h>
#include <intsafe.h>
#include <strsafe.h>
}

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <msctf.h> // Cicero header
#include <tsattrs.h> // ITextStore standard attributes

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include "../inc/contsf.h"

#include "globals.h"

#include "ConsoleTSF.h"
#include "TfCtxtComp.h"
