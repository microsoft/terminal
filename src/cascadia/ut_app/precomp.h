/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).

Author(s):
- Carlos Zamora (cazamor) April 2019
--*/

#pragma once

// Manually include til after we include Windows.Foundation to give it winrt superpowers
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

#include <WexTestClass.h>
#include <json/json.h>
#include "consoletaeftemplates.hpp"

// Are you thinking about adding WinRT things here? If so, you probably want to
// add your test to TerminalApp.LocalTests, not TerminalApp.UnitTests. The
// UnitTests run in CI, while the LocalTests do not. However, since the CI can't
// run XAML islands or unpackaged WinRT, any tests using those features will
// need to be added to the LocalTests.

// These however are okay, for some _basic_ winrt things:
#include <Unknwn.h>
#include <hstring.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>

#include <winrt/Microsoft.Terminal.Core.h>

#include "winrtTaefTemplates.hpp"

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"

// Common includes for most tests:
#include "../../inc/conattrs.hpp"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
