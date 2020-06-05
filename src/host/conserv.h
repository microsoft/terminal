/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conserv.h

Abstract:
- This module contains the include files and definitions for the console server DLL.

Author:
- Therese Stowell (ThereseS) 16-Nov-1990

Revision History:
- Many items removed into individual classes relevant to individual components (MiNiksa, PaulCam - 2014)
- Renamed from consrv.h due to naming conflict with the one published from minkernel.
--*/

#pragma once

#include "cmdline.h"
#include "globals.h"
#include "server.h"
#include "settings.hpp"
#include "tracing.hpp"

#define NT_TESTNULL(var) (((var) == nullptr) ? STATUS_NO_MEMORY : STATUS_SUCCESS)
#define NT_TESTNULL_GLE(var) (((var) == nullptr) ? NTSTATUS_FROM_WIN32(GetLastError()) : STATUS_SUCCESS);

/*
 * Used to store some console attributes for the console.  This is a means
 * to cache the color in the extra-window-bytes, so USER/KERNEL can get
 * at it for hungapp drawing.  The window-offsets are defined in NTUSER\INC.
 *
 * The other macros are just convenient means for setting the other window
 * bytes.
 */

#define PACKCOORD(pt) (MAKELONG(((pt).X), ((pt).Y)))

typedef struct _CONSOLE_API_CONNECTINFO
{
    Settings ConsoleInfo;
    BOOLEAN ConsoleApp;
    BOOLEAN WindowVisible;
    DWORD ProcessGroupId;
    DWORD TitleLength;
    WCHAR Title[MAX_PATH + 1];
    DWORD AppNameLength;
    WCHAR AppName[128];
    DWORD CurDirLength;
    WCHAR CurDir[MAX_PATH + 1];
} CONSOLE_API_CONNECTINFO, *PCONSOLE_API_CONNECTINFO;
