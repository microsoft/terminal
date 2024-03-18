/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- pch.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

#pragma once

// Ignore checked iterators warning from VC compiler.
#define _SCL_SECURE_NO_WARNINGS

// Block minwindef.h min/max macros to prevent <algorithm> conflict
#define NOMINMAX

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOHELP
#define NOCOMM

#include <unknwn.h>

#include <windows.h>
#include <UIAutomation.h>
#include <cstdlib>
#include <cstring>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <ShObjIdl.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
#include "../inc/LibraryIncludes.h"

// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <wil/cppwinrt.h>

// Needed just for XamlIslands to work at all:
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

// Additional headers for various xaml features. We need:
//  * Core so we can resume_foreground with CoreDispatcher
//  * Controls for grid
//  * Media for ScaleTransform
//  * ApplicationModel for finding the path to wt.exe
//  * Primitives for Popup (used by GetOpenPopupsForXamlRoot)
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <winrt/TerminalApp.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>
#include <winrt/Microsoft.Terminal.Remoting.h>
#include <winrt/Microsoft.Terminal.Control.h>
#include <winrt/Microsoft.Terminal.UI.h>

#include <wil/resource.h>
#include <wil/win32_helpers.h>

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
TRACELOGGING_DECLARE_PROVIDER(g_hWindowsTerminalProvider);
#include <telemetry/ProjectTelemetry.h>
#include <TraceLoggingActivity.h>

// For commandline argument processing
#include <shellapi.h>
#include <processenv.h>
#include <WinUser.h>

#include "til.h"
#include "til/mutex.h"

#include <SafeDispatcherTimer.h>

#include <cppwinrt_utils.h>
#include <wil/cppwinrt_helpers.h> // must go after the CoreDispatcher type is defined
