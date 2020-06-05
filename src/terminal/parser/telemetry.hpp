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
        static TermTelemetry& Instance() noexcept
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
            HPR,
            VPR,
            ICH,
            DCH,
            SU,
            SD,
            ANSISYSSC,
            ANSISYSRC,
            IL,
            DL,
            DECSTBM,
            NEL,
            IND,
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
            LS2,
            LS3,
            LS1R,
            LS2R,
            LS3R,
            SS2,
            SS3,
            DOCS,
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
            DECALN,
            // Only use this last enum as a count of the number of codes.
            NUMBER_OF_CODES
        };
        void Log(const Codes code) noexcept;
        void LogFailed(const wchar_t wch) noexcept;
        void SetShouldWriteFinalLog(const bool writeLog) noexcept;
        void SetActivityId(const GUID* activityId) noexcept;
        unsigned int GetAndResetTimesUsedCurrent() noexcept;
        unsigned int GetAndResetTimesFailedCurrent() noexcept;
        unsigned int GetAndResetTimesFailedOutsideRangeCurrent() noexcept;

    private:
        // Used to prevent multiple instances
        TermTelemetry() noexcept;
        ~TermTelemetry();
        TermTelemetry(TermTelemetry const&) = delete;
        TermTelemetry(TermTelemetry&&) = delete;
        TermTelemetry& operator=(const TermTelemetry&) = delete;
        TermTelemetry& operator=(TermTelemetry&&) = delete;

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
