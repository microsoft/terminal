// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "telemetry.hpp"

// This code remains to serve as template if we ever need telemetry for conhost again.
// The git history for this file may prove useful.
#if 0
Telemetry::Telemetry()
{
    TraceLoggingRegister(g_hConhostV2EventTraceProvider);
    TraceLoggingWriteStart(_activity, "ActivityStart");
}

Telemetry::~Telemetry()
{
    TraceLoggingWriteStop(_activity, "ActivityStop");
    TraceLoggingUnregister(g_hConhostV2EventTraceProvider);
}
#endif
