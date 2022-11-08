// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// We're suspending the inclusion of til here so that we can include
// it after some of our C++/WinRT headers.
#define BLOCK_TIL
#include <LibraryIncludes.h>
#include "winrt/Windows.Foundation.h"

#include "winrt/Microsoft.Terminal.Core.h"

// Including TraceLogging essentials for the binary
#include <TraceLoggingProvider.h>
#include <winmeta.h>
// We'll just use the one in TerminalControl, since that's what we'll get embedded in.
TRACELOGGING_DECLARE_PROVIDER(g_hTerminalControlProvider);
#include <telemetry/ProjectTelemetry.h>
#include <TraceLoggingActivity.h>

#include <til.h>
