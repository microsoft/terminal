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
        void TraceInvalidate(const til::rectangle view) const;
        void TraceLastText(const til::point lastText) const;
        void TraceScrollFrame(const til::point scrollDelta) const;
        void TraceMoveCursor(const til::point lastText, const til::point cursor) const;
        void TraceSetWrapped(const short wrappedRow) const;
        void TraceClearWrapped() const;
        void TraceWrapped() const;
        void TracePaintCursor(const til::point coordCursor) const;
        void TraceInvalidateAll(const til::rectangle view) const;
        void TraceTriggerCircling(const bool newFrame) const;
        void TraceInvalidateScroll(const til::point scroll) const;
        void TraceStartPaint(const bool quickReturn,
                             const til::bitmap invalidMap,
                             const til::rectangle lastViewport,
                             const til::point scrollDelta,
                             const bool cursorMoved,
                             const std::optional<short>& wrappedRow) const;
        void TraceEndPaint() const;
    };
}
