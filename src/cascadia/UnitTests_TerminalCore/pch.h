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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // If this is not defined, windows.h includes commdlg.h which defines FindText globally and conflicts with UIAutomation ITextRangeProvider.
#endif

#define NOMINMAX

// Define and then undefine WIN32_NO_STATUS because windows.h has no guard to prevent it from double defing certain statuses
// when included with ntstatus.h
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4430) // Must disable 4430 "default int" warning for C++ because ntstatus.h is inflexible SDK definition.
#include <ntstatus.h>
#pragma warning(pop)

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
// </Conhost Includes>

#include <cppwinrt_utils.h>
