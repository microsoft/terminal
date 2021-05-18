// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"

using namespace Microsoft::Console::VirtualTerminal;

#pragma warning(push)
#pragma warning(disable : 26447) // The function is declared 'noexcept' but calls function '_tlgWrapBinary<wchar_t>()' which may throw exceptions
#pragma warning(disable : 26477) // Use 'nullptr' rather than 0 or NULL

ParserTracing::ParserTracing() noexcept
{
    ClearSequenceTrace();
}

void ParserTracing::TraceStateChange(const std::wstring_view name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_EnterState",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceOnAction(const std::wstring_view name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Action",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
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

void ParserTracing::TraceOnEvent(const std::wstring_view name) const noexcept
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Event",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                      TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

void ParserTracing::TraceCharInput(const wchar_t wch)
{
    AddSequenceTrace(wch);
    const auto sch = gsl::narrow_cast<INT16>(wch);

    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_NewChar",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
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
void ParserTracing::DispatchPrintRunTrace(const std::wstring_view string) const
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
