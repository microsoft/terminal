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

#define _OLEAUT32_
#include <windows.h>
#include <ole2.h>

extern "C" {
#include <winuser.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <ime.h>
#include <strsafe.h>
#include <intsafe.h>
}

#include <msctf.h> // Cicero header
#include <tsattrs.h> // ITextStore standard attributes

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include "..\inc\contsf.h"

#include "globals.h"

#include "TfCtxtComp.h"
#include "ConsoleTSF.h"
