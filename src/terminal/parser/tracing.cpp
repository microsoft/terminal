// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"

using namespace Microsoft::Console::VirtualTerminal;

#pragma warning(push)
#pragma warning(disable : 26447) // The function is declared 'noexcept' but calls function '_tlgWrapBinary<wchar_t>()' which may throw exceptions
#pragma warning(disable : 26477) // Use 'nullptr' rather than 0 or NULL

TRACELOGGING_DEFINE_PROVIDER(g_hConsoleVirtTermParserEventTraceProvider,
                             "Microsoft.Windows.Console.VirtualTerminal.Parser",
                             // {c9ba2a84-d3ca-5e19-2bd6-776a0910cb9d}
                             (0xc9ba2a84, 0xd3ca, 0x5e19, 0x2b, 0xd6, 0x77, 0x6a, 0x09, 0x10, 0xcb, 0x9d));

void ParserTracing::TraceStateChange(_In_z_ const wchar_t* name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_EnterState",
                      TraceLoggingWideString(name),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceOnAction(_In_z_ const wchar_t* name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Action",
                      TraceLoggingWideString(name),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceOnExecute(const wchar_t wch) const noexcept
{
    const auto sch = gsl::narrow_cast<INT16>(wch);
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Execute",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceOnExecuteFromEscape(const wchar_t wch) const noexcept
{
    const auto sch = gsl::narrow_cast<INT16>(wch);
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_ExecuteFromEscape",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceOnEvent(_In_z_ const wchar_t* name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Event",
                      TraceLoggingWideString(name),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceCharInput(const wchar_t wch)
{
    AddSequenceTrace(wch);

    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_NewChar",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(gsl::narrow_cast<INT16>(wch)),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::AddSequenceTrace(const wchar_t wch)
{
    // Don't waste time storing this if no one is listening.
    if (TraceLoggingProviderEnabled(g_hConsoleVirtTermParserEventTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        _sequenceTrace.push_back(wch);
    }
}

void ParserTracing::DispatchSequenceTrace(const bool fSuccess) noexcept
{
    if (fSuccess)
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_OK",
                          TraceLoggingWideString(_sequenceTrace.c_str()),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
    else
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_FAIL",
                          TraceLoggingWideString(_sequenceTrace.c_str()),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }

    ClearSequenceTrace();
}

void ParserTracing::ClearSequenceTrace() noexcept
{
    _sequenceTrace.clear();
}

// NOTE: I'm expecting this to not be null terminated
void ParserTracing::DispatchPrintRunTrace(const std::wstring_view& string) const
{
    if (string.size() == 1)
    {
        const auto wch = til::at(string, 0);
        const auto sch = gsl::narrow_cast<INT16>(wch);
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_PrintRun",
                          TraceLoggingWChar(wch),
                          TraceLoggingHexInt16(sch),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
    else
    {
        const auto length = gsl::narrow_cast<ULONG>(string.size());

        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_PrintRun",
                          TraceLoggingCountedWideString(string.data(), length),
                          TraceLoggingValue(length),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

#pragma warning(pop)
