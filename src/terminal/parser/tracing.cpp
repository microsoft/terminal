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

void ParserTracing::TraceStateChange(_In_ PCWSTR const pwszName) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_EnterState",
                      TraceLoggingWideString(pwszName),
                      TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
}

void ParserTracing::TraceOnAction(_In_ PCWSTR const pwszName) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Action",
                      TraceLoggingWideString(pwszName),
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

void ParserTracing::TraceOnEvent(_In_ PCWSTR const pwszName) const
{
    TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                      "StateMachine_Event",
                      TraceLoggingWideString(pwszName),
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
    // -1 to always leave the last character as null/0.
    if (_cchSequenceTrace < s_cMaxSequenceTrace - 1)
    {
        _rgwchSequenceTrace[_cchSequenceTrace] = wch;
        _cchSequenceTrace++;
    }
}

void ParserTracing::DispatchSequenceTrace(const bool fSuccess)
{
    if (fSuccess)
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_OK",
                          TraceLoggingWideString(_rgwchSequenceTrace),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }
    else
    {
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_Sequence_FAIL",
                          TraceLoggingWideString(_rgwchSequenceTrace),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }

    ClearSequenceTrace();
}

void ParserTracing::ClearSequenceTrace()
{
    ZeroMemory(_rgwchSequenceTrace, sizeof(_rgwchSequenceTrace));
    _cchSequenceTrace = 0;
}

// NOTE: I'm expecting this to not be null terminated
void ParserTracing::DispatchPrintRunTrace(const wchar_t* const pwsString, const size_t cchString) const
{
    size_t charsRemaining = cchString;
    wchar_t str[BYTE_MAX + 4 + sizeof(wchar_t) + sizeof('\0')];

    if (cchString == 1)
    {
        wchar_t wch = *pwsString;
        INT16 sch = (INT16)wch;
        TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                          "StateMachine_PrintRun",
                          TraceLoggingWChar(wch),
                          TraceLoggingHexInt16(sch),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
    }
    else
    {
        while (charsRemaining > 0)
        {
            size_t strLen = 0;
            if (charsRemaining > ARRAYSIZE(str) - 1)
            {
                strLen = ARRAYSIZE(str) - 1;
            }
            else
            {
                strLen = charsRemaining;
            }
            charsRemaining -= strLen;

            memcpy(str, pwsString, sizeof(wchar_t) * strLen);
            str[strLen] = '\0';

            TraceLoggingWrite(g_hConsoleVirtTermParserEventTraceProvider,
                              "StateMachine_PrintRun",
                              TraceLoggingWideString(str),
                              TraceLoggingValue(strLen),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE));
        }
    }
}
