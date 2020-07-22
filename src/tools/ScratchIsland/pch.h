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
#include <unknwn.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#include <windows.h>
#include <UIAutomation.h>
#include <cstdlib>
#include <cstring>
#include <shellscalingapi.h>
#include <windowsx.h>

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
#include <winrt/Windows.system.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/windows.ui.core.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

// Additional headers for various xaml features. We need:
//  * Controls for grid
//  * Media for ScaleTransform
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.ui.xaml.media.h>
#include <windows.ui.xaml.media.dxinterop.h>

#include <wil/resource.h>
#include <wil/win32_helpers.h>

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
TRACELOGGING_DECLARE_PROVIDER(g_hWindowsTerminalProvider);
#include <telemetry\ProjectTelemetry.h>
#include <TraceLoggingActivity.h>

// For commandline argument processing
#include <shellapi.h>
#include <processenv.h>

#include <dcomp.h>

#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1_2.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include "til.h"
