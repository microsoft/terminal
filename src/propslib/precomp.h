// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define DEFINE_CONSOLEV2_PROPERTIES
#define INC_OLE2

#define NOMINMAX

#include <windows.h>

#ifdef EXTERNAL_BUILD
#include <ShlObj.h>
#else
#include <shlobj_core.h>
#endif

#include <strsafe.h>
#include <sal.h>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <winconp.h>
#include "../host/settings.hpp"
#include <pathcch.h>

#include "conpropsp.hpp"

#pragma region Definitions from DDK(wdm.h)
FORCEINLINE
PSINGLE_LIST_ENTRY
PopEntryList(
    _Inout_ PSINGLE_LIST_ENTRY ListHead)
{
    PSINGLE_LIST_ENTRY FirstEntry;

    FirstEntry = ListHead->Next;
    if (FirstEntry != nullptr)
    {
        ListHead->Next = FirstEntry->Next;
    }

    return FirstEntry;
}

FORCEINLINE
VOID PushEntryList(
    _Inout_ PSINGLE_LIST_ENTRY ListHead,
    _Inout_ __drv_aliasesMem PSINGLE_LIST_ENTRY Entry)

{
    Entry->Next = ListHead->Next;
    ListHead->Next = Entry;
    return;
}
#pragma endregion
