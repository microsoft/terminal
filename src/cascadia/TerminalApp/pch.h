// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOHELP
#define NOCOMM

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
#include <LibraryIncludes.h>
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

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/windows.ui.core.h>
#include <winrt/Windows.ui.input.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.ui.xaml.media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.ui.xaml.input.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Documents.h"
#include "winrt/Windows.UI.Xaml.Automation.h"
#include "winrt/Windows.UI.Xaml.Automation.Peers.h"
#include "winrt/Windows.UI.ViewManagement.h"
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <winrt/Microsoft.Toolkit.Win32.UI.XamlHost.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <windows.ui.xaml.media.dxinterop.h>

#include <winrt/Windows.System.h>

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
TRACELOGGING_DECLARE_PROVIDER(g_hTerminalAppProvider);
#include <telemetry/ProjectTelemetry.h>
#include <TraceLoggingActivity.h>

#include <shellapi.h>
#include <shobjidl_core.h>

#include <winrt/Microsoft.Terminal.Core.h>
#include <winrt/Microsoft.Terminal.Control.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>

#include <winrt/Windows.UI.Popups.h>

#include <CLI11/CLI11.hpp>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"
