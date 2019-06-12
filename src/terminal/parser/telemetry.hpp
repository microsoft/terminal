// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- telemetry.hpp

Abstract:
- This module is used for recording all telemetry feedback from the console virtual terminal parser
*/
#pragma once

// Including TraceLogging essentials for the binary
#include <windows.h>
#include <winmeta.h>
#include <TraceLoggingProvider.h>
#include "limits.h"

TRACELOGGING_DECLARE_PROVIDER(g_hConsoleVirtTermParserEventTraceProvider);

namespace Microsoft::Console::VirtualTerminal
{
    class TermTelemetry sealed
    {
    public:
        // Implement this as a singleton class.
        static TermTelemetry& Instance()
        {
            static TermTelemetry s_Instance;
            return s_Instance;
        }

        // Names primarily from http://inwap.com/pdp10/ansicode.txt
        enum Codes
        {
            CUU = 0,
            CUD,
            CUF,
            CUB,
            CNL,
            CPL,
            CHA,
            CUP,
            ED,
            EL,
            SGR,
            DECSC,
            DECRC,
            DECSET,
            DECRST,
            DECKPAM,
            DECKPNM,
            DSR,
            DA,
            VPA,
            ICH,
            DCH,
            SU,
            SD,
            ANSISYSSC,
            ANSISYSRC,
            IL,
            DL,
            DECSTBM,
            RI,
            OSCWT,
            HTS,
            CHT,
            CBT,
            TBC,
            ECH,
            DesignateG0,
            DesignateG1,
            DesignateG2,
            DesignateG3,
            HVP,
            DECSTR,
            RIS,
            DECSCUSR,
            DTTERM_WM,
            OSCCT,
            OSCSCC,
            OSCRCC,
            REP,
            OSCFG,
            OSCBG,
            // Only use this last enum as a count of the number of codes.
            NUMBER_OF_CODES
        };
        void Log(const Codes code);
        void LogFailed(const wchar_t wch);
        void SetShouldWriteFinalLog(const bool writeLog);
        void SetActivityId(const GUID* activityId);
        unsigned int GetAndResetTimesUsedCurrent();
        unsigned int GetAndResetTimesFailedCurrent();
        unsigned int GetAndResetTimesFailedOutsideRangeCurrent();

    private:
        // Used to prevent multiple instances
        TermTelemetry();
        ~TermTelemetry();
        TermTelemetry(TermTelemetry const&);
        void operator=(TermTelemetry const&);

        void WriteFinalTraceLog() const;

        unsigned int _uiTimesUsedCurrent;
        unsigned int _uiTimesFailedCurrent;
        unsigned int _uiTimesFailedOutsideRangeCurrent;
        unsigned int _uiTimesUsed[NUMBER_OF_CODES];
        unsigned int _uiTimesFailed[CHAR_MAX + 1];
        unsigned int _uiTimesFailedOutsideRange;
        GUID _activityId;

        bool _fShouldWriteFinalLog;
    };
}
