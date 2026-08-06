/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- tracing.hpp

Abstract:
- This module is used for recording tracing/debugging information to the telemetry ETW channel
--*/

#pragma once
#include <string>
#include <windows.h>
#include <winmeta.h>
#include <TraceLoggingProvider.h>
#include <telemetry/ProjectTelemetry.h>

TRACELOGGING_DECLARE_PROVIDER(g_hCTerminalCoreProvider);
