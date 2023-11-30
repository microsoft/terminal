// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

// This code remains to serve as template if we ever need telemetry for conhost again.
// The git history for this file may prove useful.
#if 0
class Telemetry
{
public:
    Telemetry();
    ~Telemetry();

private:
    TraceLoggingActivity<g_hConhostV2EventTraceProvider> _activity;
};
#endif
