// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

// Ignore checked iterators warning from VC compiler.
#define _SCL_SECURE_NO_WARNINGS

// Block minwindef.h min/max macros to prevent <algorithm> conflict
#define NOMINMAX

#define WIN32_LEAN_AND_MEAN

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <shellscalingapi.h>


#include <LibraryIncludes.h>
// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <unknwn.h>

#include <hstring.h>

#include <winrt/Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/windows.ui.core.h>
#include <winrt/Windows.ui.input.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.ui.xaml.media.h>
#include <winrt/Windows.ui.xaml.media.imaging.h>
#include <winrt/Windows.ui.xaml.input.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Documents.h"

#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <windows.ui.xaml.media.dxinterop.h>

#include <winrt/Windows.System.h>

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
TRACELOGGING_DECLARE_PROVIDER(g_hTerminalWin32Provider);
#include <telemetry\ProjectTelemetry.h>
#include <TraceLoggingActivity.h>

// JsonCpp
#include <json.h>

#include <shellapi.h>
#include <filesystem>
