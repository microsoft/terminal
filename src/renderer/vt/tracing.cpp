// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"
#include <sstream>

TRACELOGGING_DEFINE_PROVIDER(g_hConsoleVtRendererTraceProvider,
                             "Microsoft.Windows.Console.Render.VtEngine",
                             // tl:{c9ba2a95-d3ca-5e19-2bd6-776a0910cb9d}
                             (0xc9ba2a95, 0xd3ca, 0x5e19, 0x2b, 0xd6, 0x77, 0x6a, 0x09, 0x10, 0xcb, 0x9d),
                             TraceLoggingOptionMicrosoftTelemetry());

using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Types;

RenderTracing::RenderTracing()
{
#ifndef UNIT_TESTING
    TraceLoggingRegister(g_hConsoleVtRendererTraceProvider);
#endif UNIT_TESTING
}

RenderTracing::~RenderTracing()
{
#ifndef UNIT_TESTING
    TraceLoggingUnregister(g_hConsoleVtRendererTraceProvider);
#endif UNIT_TESTING
}

// Function Description:
// - Convert the string to only have printable characters in it. Control
//      characters are converted to hat notation, spaces are converted to "SPC"
//      (to be able to see them at the end of a string), and DEL is written as
//      "\x7f".
// Arguments:
// - inString: The string to convert
// Return Value:
// - a string with only printable characters in it.
std::string toPrintableString(const std::string_view& inString)
{
    std::string retval = "";
    for (size_t i = 0; i < inString.length(); i++)
    {
        unsigned char c = inString[i];
        if (c < '\x20')
        {
            retval += "^";
            char actual = (c + 0x40);
            retval += std::string(1, actual);
        }
        else if (c == '\x7f')
        {
            retval += "\\x7f";
        }
        else if (c == '\x20')
        {
            retval += "SPC";
        }
        else
        {
            retval += std::string(1, c);
        }
    }
    return retval;
}
void RenderTracing::TraceString(const std::string_view& instr) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const std::string _seq = toPrintableString(instr);
        const char* const seq = _seq.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceString",
                          TraceLoggingUtf8String(seq),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(instr);
#endif UNIT_TESTING
}

void RenderTracing::TraceInvalidate(const til::rectangle invalidRect) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto invalidatedStr = invalidRect.to_string();
        const auto invalidated = invalidatedStr.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceInvalidate",
                          TraceLoggingWideString(invalidated),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(invalidRect);
#endif UNIT_TESTING
}

void RenderTracing::TraceInvalidateAll(const til::rectangle viewport) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto invalidatedStr = viewport.to_string();
        const auto invalidatedAll = invalidatedStr.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceInvalidateAll",
                          TraceLoggingWideString(invalidatedAll),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(viewport);
#endif UNIT_TESTING
}

void RenderTracing::TraceTriggerCircling(const bool newFrame) const
{
#ifndef UNIT_TESTING
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceTriggerCircling",
                      TraceLoggingBool(newFrame),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
#else
    UNREFERENCED_PARAMETER(newFrame);
#endif UNIT_TESTING
}

void RenderTracing::TraceInvalidateScroll(const til::point scroll) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto scrollDeltaStr = scroll.to_string();
        const auto scrollDelta = scrollDeltaStr.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceInvalidateScroll",
                          TraceLoggingWideString(scrollDelta),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(scroll);
#endif
}

void RenderTracing::TraceStartPaint(const bool quickReturn,
                                    const til::pmr::bitmap& invalidMap,
                                    const til::rectangle lastViewport,
                                    const til::point scrollDelt,
                                    const bool cursorMoved,
                                    const std::optional<short>& wrappedRow) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto invalidatedStr = invalidMap.to_string();
        const auto invalidated = invalidatedStr.c_str();
        const auto lastViewStr = lastViewport.to_string();
        const auto lastView = lastViewStr.c_str();
        const auto scrollDeltaStr = scrollDelt.to_string();
        const auto scrollDelta = scrollDeltaStr.c_str();
        if (wrappedRow.has_value())
        {
            TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                              "VtEngine_TraceStartPaint",
                              TraceLoggingBool(quickReturn),
                              TraceLoggingWideString(invalidated),
                              TraceLoggingWideString(lastView),
                              TraceLoggingWideString(scrollDelta),
                              TraceLoggingBool(cursorMoved),
                              TraceLoggingValue(wrappedRow.value()),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
        else
        {
            TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                              "VtEngine_TraceStartPaint",
                              TraceLoggingBool(quickReturn),
                              TraceLoggingWideString(invalidated),
                              TraceLoggingWideString(lastView),
                              TraceLoggingWideString(scrollDelta),
                              TraceLoggingBool(cursorMoved),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }
#else
    UNREFERENCED_PARAMETER(quickReturn);
    UNREFERENCED_PARAMETER(invalidMap);
    UNREFERENCED_PARAMETER(lastViewport);
    UNREFERENCED_PARAMETER(scrollDelt);
    UNREFERENCED_PARAMETER(cursorMoved);
    UNREFERENCED_PARAMETER(wrappedRow);
#endif UNIT_TESTING
}

void RenderTracing::TraceEndPaint() const
{
#ifndef UNIT_TESTING
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceEndPaint",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
#else
#endif UNIT_TESTING
}

void RenderTracing::TraceLastText(const til::point lastTextPos) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto lastTextStr = lastTextPos.to_string();
        const auto lastText = lastTextStr.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceLastText",
                          TraceLoggingWideString(lastText),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(lastTextPos);
#endif UNIT_TESTING
}

void RenderTracing::TraceScrollFrame(const til::point scrollDeltaPos) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto scrollDeltaStr = scrollDeltaPos.to_string();
        const auto scrollDelta = scrollDeltaStr.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceScrollFrame",
                          TraceLoggingWideString(scrollDelta),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(scrollDeltaPos);
#endif UNIT_TESTING
}

void RenderTracing::TraceMoveCursor(const til::point lastTextPos, const til::point cursor) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto lastTextStr = lastTextPos.to_string();
        const auto lastText = lastTextStr.c_str();

        const auto cursorStr = cursor.to_string();
        const auto cursorPos = cursorStr.c_str();

        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceMoveCursor",
                          TraceLoggingWideString(lastText),
                          TraceLoggingWideString(cursorPos),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(lastTextPos);
    UNREFERENCED_PARAMETER(cursor);
#endif UNIT_TESTING
}

void RenderTracing::TraceWrapped() const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto* const msg = "Wrapped instead of \\r\\n";
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceWrapped",
                          TraceLoggingString(msg),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
#endif UNIT_TESTING
}

void RenderTracing::TraceSetWrapped(const short wrappedRow) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceSetWrapped",
                          TraceLoggingValue(wrappedRow),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(wrappedRow);
#endif UNIT_TESTING
}

void RenderTracing::TraceClearWrapped() const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto* const msg = "Cleared wrap state";
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TraceClearWrapped",
                          TraceLoggingString(msg),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
#endif UNIT_TESTING
}

void RenderTracing::TracePaintCursor(const til::point coordCursor) const
{
#ifndef UNIT_TESTING
    if (TraceLoggingProviderEnabled(g_hConsoleVtRendererTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        const auto cursorPosString = coordCursor.to_string();
        const auto cursorPos = cursorPosString.c_str();
        TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                          "VtEngine_TracePaintCursor",
                          TraceLoggingWideString(cursorPos),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
#else
    UNREFERENCED_PARAMETER(coordCursor);
#endif UNIT_TESTING
}
