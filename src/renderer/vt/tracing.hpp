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
#include "../../types/inc/Viewport.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hConsoleVtRendererTraceProvider);

namespace Microsoft::Console::VirtualTerminal
{
    class RenderTracing final
    {
    public:
        RenderTracing();
        ~RenderTracing();
        void TraceStringFill(const size_t n, const char c) const;
        void TraceString(const std::string_view& str) const;
        void TraceInvalidate(const til::rect& view) const;
        void TraceLastText(const til::point lastText) const;
        void TraceScrollFrame(const til::point scrollDelta) const;
        void TraceMoveCursor(const til::point lastText, const til::point cursor) const;
        void TraceSetWrapped(const til::CoordType wrappedRow) const;
        void TraceClearWrapped() const;
        void TraceWrapped() const;
        void TracePaintCursor(const til::point coordCursor) const;
        void TraceInvalidateAll(const til::rect& view) const;
        void TraceTriggerCircling(const bool newFrame) const;
        void TraceInvalidateScroll(const til::point scroll) const;
        void TraceStartPaint(const bool quickReturn,
                             const til::pmr::bitmap& invalidMap,
                             const til::rect& lastViewport,
                             const til::point scrollDelta,
                             const bool cursorMoved,
                             const std::optional<til::CoordType>& wrappedRow) const;
        void TraceEndPaint() const;
    };
}
