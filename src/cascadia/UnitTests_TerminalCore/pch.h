/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- pch.h

Abstract:
- Contains external headers to include in the precompile phase of console build
  process.
- Avoid including internal project headers. Instead include them only in the
  classes that need them (helps with test project building).

Author(s):
- Carlos Zamora (cazamor) April 2019
--*/

#pragma once

#define BLOCK_TIL
// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"
// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <wil/cppwinrt.h>
#include <unknwn.h>
#include <hstring.h>

#include <WexTestClass.h>
#include "consoletaeftemplates.hpp"
#include "winrtTaefTemplates.hpp"

#include <winrt/Windows.system.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Microsoft.Terminal.Core.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"

// <Conhost includes>
// These are needed because the roundtrip tests included in this library also
// re-use some conhost code that depends on these.

#include "conddkrefs.h"
// From ntdef.h, but that can't be included or it'll fight over PROBE_ALIGNMENT and other such arch specific defs
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
/*lint -save -e624 */ // Don't complain about different typedefs.
typedef NTSTATUS* PNTSTATUS;
/*lint -restore */ // Resume checking for different typedefs.
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
// </Conhost Includes>
