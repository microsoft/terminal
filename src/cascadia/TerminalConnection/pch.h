// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

// Needs to be defined or we get redeclaration errors
#define WIN32_LEAN_AND_MEAN

#define BLOCK_GSL

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
#include <LibraryIncludes.h>

// Must be included before any WinRT headers.
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <wil/cppwinrt.h>

#include "winrt/Windows.Security.Credentials.h"
#include "winrt/Windows.Foundation.Collections.h"
#include <Windows.h>

#include <TraceLoggingProvider.h>
TRACELOGGING_DECLARE_PROVIDER(g_hTerminalConnectionProvider);
#include <telemetry/ProjectTelemetry.h>

#include "til.h"
