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
    const std::string _seq = toPrintableString(instr);
    const char* const seq = _seq.c_str();
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceString",
                      TraceLoggingString(seq),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
    UNREFERENCED_PARAMETER(instr);
#endif UNIT_TESTING
}

std::string _ViewportToString(const Viewport& view)
{
    std::stringstream ss;
    ss << "{LT:(";
    ss << view.Left();
    ss << ",";
    ss << view.Top();
    ss << ") RB:(";
    ss << view.RightInclusive();
    ss << ",";
    ss << view.BottomInclusive();
    ss << ") [";
    ss << view.Width();
    ss << "x";
    ss << view.Height();
    ss << "]}";
    std::string myString = "";
    const auto s = ss.str();
    myString += s;
    return myString;
}

std::string _CoordToString(const COORD& c)
{
    std::stringstream ss;
    ss << "{";
    ss << c.X;
    ss << ",";
    ss << c.Y;
    ss << "}";
    const auto s = ss.str();
    // Make sure you actually place this value in a local after calling, don't
    // just inline it to _CoordToString().c_str()
    return s;
}

void RenderTracing::TraceInvalidate(const Viewport invalidRect) const
{
#ifndef UNIT_TESTING
    const auto invalidatedStr = _ViewportToString(invalidRect);
    const auto invalidated = invalidatedStr.c_str();
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceInvalidate",
                      TraceLoggingString(invalidated),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
    UNREFERENCED_PARAMETER(invalidRect);
#endif UNIT_TESTING
}

void RenderTracing::TraceInvalidateAll(const Viewport viewport) const
{
#ifndef UNIT_TESTING
    const auto invalidatedStr = _ViewportToString(viewport);
    const auto invalidatedAll = invalidatedStr.c_str();
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceInvalidateAll",
                      TraceLoggingString(invalidatedAll),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
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
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
    UNREFERENCED_PARAMETER(newFrame);
#endif UNIT_TESTING
}

void RenderTracing::TraceStartPaint(const bool quickReturn,
                                    const bool invalidRectUsed,
                                    const Microsoft::Console::Types::Viewport invalidRect,
                                    const Microsoft::Console::Types::Viewport lastViewport,
                                    const COORD scrollDelt,
                                    const bool cursorMoved) const
{
#ifndef UNIT_TESTING
    const auto invalidatedStr = _ViewportToString(invalidRect);
    const auto invalidated = invalidatedStr.c_str();
    const auto lastViewStr = _ViewportToString(lastViewport);
    const auto lastView = lastViewStr.c_str();
    const auto scrollDeltaStr = _CoordToString(scrollDelt);
    const auto scrollDelta = scrollDeltaStr.c_str();
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceStartPaint",
                      TraceLoggingBool(quickReturn),
                      TraceLoggingBool(invalidRectUsed),
                      TraceLoggingString(invalidated),
                      TraceLoggingString(lastView),
                      TraceLoggingString(scrollDelta),
                      TraceLoggingBool(cursorMoved),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
    UNREFERENCED_PARAMETER(quickReturn);
    UNREFERENCED_PARAMETER(invalidRectUsed);
    UNREFERENCED_PARAMETER(invalidRect);
    UNREFERENCED_PARAMETER(lastViewport);
    UNREFERENCED_PARAMETER(scrollDelt);
    UNREFERENCED_PARAMETER(cursorMoved);
#endif UNIT_TESTING
}

void RenderTracing::TraceEndPaint() const
{
#ifndef UNIT_TESTING
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceEndPaint",
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
#endif UNIT_TESTING
}

void RenderTracing::TraceLastText(const COORD lastTextPos) const
{
#ifndef UNIT_TESTING
    const auto lastTextStr = _CoordToString(lastTextPos);
    const auto lastText = lastTextStr.c_str();
    TraceLoggingWrite(g_hConsoleVtRendererTraceProvider,
                      "VtEngine_TraceLastText",
                      TraceLoggingString(lastText),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
#else
    UNREFERENCED_PARAMETER(lastTextPos);
#endif UNIT_TESTING
}
