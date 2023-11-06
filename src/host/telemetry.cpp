// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "telemetry.hpp"

TRACELOGGING_DEFINE_PROVIDER(g_hConhostV2EventTraceProvider,
                             "Microsoft.Windows.Console.Host",
                             // {fe1ff234-1f09-50a8-d38d-c44fab43e818}
                             (0xfe1ff234, 0x1f09, 0x50a8, 0xd3, 0x8d, 0xc4, 0x4f, 0xab, 0x43, 0xe8, 0x18),
                             TraceLoggingOptionMicrosoftTelemetry());

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
