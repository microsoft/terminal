// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define DEFINE_CONSOLEV2_PROPERTIES
#define INC_OLE2

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// From ntdef.h, but that can't be included or it'll fight over PROBE_ALIGNMENT and other such arch specific defs
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
/*lint -save -e624 */ // Don't complain about different typedefs.
typedef NTSTATUS* PNTSTATUS;
/*lint -restore */ // Resume checking for different typedefs.
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#define INLINE_NTSTATUS_FROM_WIN32 1 // Must use inline NTSTATUS or it will call the wrapped function twice.
#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

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
#include "..\host\settings.hpp"
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
    if (FirstEntry != NULL)
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
