// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

// clang-format off

// Define and then undefine WIN32_NO_STATUS because windows.h has no guard to prevent it from double defing certain statuses
// when included with ntstatus.h
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// From ntdef.h, but that can't be included or it'll fight over PROBE_ALIGNMENT and other such arch specific defs
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
/*lint -save -e624 */  // Don't complain about different typedefs.
typedef NTSTATUS *PNTSTATUS;
/*lint -restore */  // Resume checking for different typedefs.
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// End From ntdef.h

#define INLINE_NTSTATUS_FROM_WIN32 1 // Must use inline NTSTATUS or it will call the wrapped function twice.
#pragma warning(push)
#pragma warning(disable:4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

#include <initguid.h>

#ifdef EXTERNAL_BUILD
#include <ShlObj.h>
#else
#include <shlobj_core.h>
#endif

#include <winuser.h>

#include <cwchar>

// Only remaining item from w32gdip.h. There's probably a better way to do this as well.
#define IS_ANY_DBCS_CHARSET( CharSet )                              \
                   ( ((CharSet) == SHIFTJIS_CHARSET)    ? TRUE :    \
                     ((CharSet) == HANGEUL_CHARSET)     ? TRUE :    \
                     ((CharSet) == CHINESEBIG5_CHARSET) ? TRUE :    \
                     ((CharSet) == GB2312_CHARSET)      ? TRUE : FALSE )

#include "conddkrefs.h"
#include "conwinuserrefs.h"

// clang-format on
