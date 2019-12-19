// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"

using namespace Microsoft::Console::VirtualTerminal;

ParserTracing::ParserTracing()
{
    ClearSequenceTrace();
}

ParserTracing::~ParserTracing()
{
}

void ParserTracing::TraceStateChange(const std::wstring_view name) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_EnterState",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceOnAction(const std::wstring_view name) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Action",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceOnExecute(const wchar_t wch) const
{
    INT16 sch = (INT16)wch;
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Execute",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceOnExecuteFromEscape(const wchar_t wch) const
{
    INT16 sch = (INT16)wch;
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_ExecuteFromEscape",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceOnEvent(const std::wstring_view name) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Event",
                      TraceLoggingCountedWideString(name.data(), gsl::narrow_cast<ULONG>(name.size())),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceCharInput(const wchar_t wch)
{
    AddSequenceTrace(wch);
    INT16 sch = (INT16)wch;

    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_NewChar",
                      TraceLoggingWChar(wch),
                      TraceLoggingHexInt16(sch),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::AddSequenceTrace(const wchar_t wch)
{
    _sequenceTrace.push_back(wch);
}

void ParserTracing::DispatchSequenceTrace(const bool fSuccess)
{
    if (fSuccess)
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_OK",
                          TraceLoggingWideString(_sequenceTrace.c_str()),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }
    else
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_FAIL",
                          TraceLoggingWideString(_sequenceTrace.c_str()),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }

    ClearSequenceTrace();
}

void ParserTracing::ClearSequenceTrace()
{
    _sequenceTrace.clear();
}

// NOTE: I'm expecting this to not be null terminated
void ParserTracing::DispatchPrintRunTrace(const std::wstring_view string) const
{
    if (string.size() == 1)
    {
        wchar_t wch = string.front();
        INT16 sch = (INT16)wch;
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_PrintRun",
                          TraceLoggingWChar(wch),
                          TraceLoggingHexInt16(sch),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }
    else
    {
        const auto length = gsl::narrow_cast<ULONG>(string.size());

        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_PrintRun",
                          TraceLoggingCountedWideString(string.data(), length),
                          TraceLoggingValue(length),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }
}
