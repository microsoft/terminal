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
#include <telemetry\ProjectTelemetry.h>
#include "../../types/inc/Viewport.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hConsoleVtRendererTraceProvider);

namespace Microsoft::Console::VirtualTerminal
{
    class RenderTracing final
    {
    public:
        RenderTracing();
        ~RenderTracing();
        void TraceString(const std::string_view& str) const;
        void TraceInvalidate(const Microsoft::Console::Types::Viewport view) const;
        void TraceLastText(const COORD lastText) const;
        void TraceInvalidateAll(const Microsoft::Console::Types::Viewport view) const;
        void TraceTriggerCircling(const bool newFrame) const;
        void TraceStartPaint(const bool quickReturn,
                             const bool invalidRectUsed,
                             const Microsoft::Console::Types::Viewport invalidRect,
                             const Microsoft::Console::Types::Viewport lastViewport,
                             const COORD scrollDelta,
                             const bool cursorMoved) const;
        void TraceEndPaint() const;
    };
}
