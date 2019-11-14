// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- stateMachine.hpp

Abstract:
- This declares the entire state machine for handling Virtual Terminal Sequences
- The design is based from the specifications at http://vt100.net
- The actual implementation of actions decoded by the StateMachine should be
  implemented in an IStateMachineEngine.
*/

#pragma once

#include "IStateMachineEngine.hpp"
#include "telemetry.hpp"
#include "tracing.hpp"
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    class StateMachine final
    {
#ifdef UNIT_TESTING
        friend class OutputEngineTest;
        friend class InputEngineTest;
#endif

    public:
        StateMachine(IStateMachineEngine* const pEngine);

        void ProcessCharacter(const wchar_t wch);
        void ProcessString(const wchar_t* const rgwch, const size_t cch);
        void ProcessString(const std::wstring& wstr);

        void ResetState();

        bool FlushToTerminal();

        const IStateMachineEngine& Engine() const noexcept;
        IStateMachineEngine& Engine() noexcept;

        static const short s_cIntermediateMax = 1;
        static const short s_cParamsMax = 16;
        static const short s_cOscStringMaxLength = 256;

    private:
        static bool s_IsActionableFromGround(const wchar_t wch);
        static bool s_IsC0Code(const wchar_t wch);
        static bool s_IsC1Csi(const wchar_t wch);
        static bool s_IsIntermediate(const wchar_t wch);
        static bool s_IsDelete(const wchar_t wch);
        static bool s_IsEscape(const wchar_t wch);
        static bool s_IsCsiIndicator(const wchar_t wch);
        static bool s_IsCsiDelimiter(const wchar_t wch);
        static bool s_IsCsiParamValue(const wchar_t wch);
        static bool s_IsCsiPrivateMarker(const wchar_t wch);
        static bool s_IsCsiInvalid(const wchar_t wch);
        static bool s_IsOscIndicator(const wchar_t wch);
        static bool s_IsOscDelimiter(const wchar_t wch);
        static bool s_IsOscParamValue(const wchar_t wch);
        static bool s_IsOscInvalid(const wchar_t wch);
        static bool s_IsOscTerminator(const wchar_t wch);
        static bool s_IsOscTerminationInitiator(const wchar_t wch);
        static bool s_IsDesignateCharsetIndicator(const wchar_t wch);
        static bool s_IsCharsetCode(const wchar_t wch);
        static bool s_IsNumber(const wchar_t wch);
        static bool s_IsSs3Indicator(const wchar_t wch);

        void _ActionExecute(const wchar_t wch);
        void _ActionExecuteFromEscape(const wchar_t wch);
        void _ActionPrint(const wchar_t wch);
        void _ActionEscDispatch(const wchar_t wch);
        void _ActionCollect(const wchar_t wch);
        void _ActionParam(const wchar_t wch);
        void _ActionCsiDispatch(const wchar_t wch);
        void _ActionOscParam(const wchar_t wch);
        void _ActionOscPut(const wchar_t wch);
        void _ActionOscDispatch(const wchar_t wch);
        void _ActionSs3Dispatch(const wchar_t wch);

        void _ActionClear();
        void _ActionIgnore();

        void _EnterGround();
        void _EnterEscape();
        void _EnterEscapeIntermediate();
        void _EnterCsiEntry();
        void _EnterCsiParam();
        void _EnterCsiIgnore();
        void _EnterCsiIntermediate();
        void _EnterOscParam();
        void _EnterOscString();
        void _EnterOscTermination();
        void _EnterSs3Entry();
        void _EnterSs3Param();

        void _EventGround(const wchar_t wch);
        void _EventEscape(const wchar_t wch);
        void _EventEscapeIntermediate(const wchar_t wch);
        void _EventCsiEntry(const wchar_t wch);
        void _EventCsiIntermediate(const wchar_t wch);
        void _EventCsiIgnore(const wchar_t wch);
        void _EventCsiParam(const wchar_t wch);
        void _EventOscParam(const wchar_t wch);
        void _EventOscString(const wchar_t wch);
        void _EventOscTermination(const wchar_t wch);
        void _EventSs3Entry(const wchar_t wch);
        void _EventSs3Param(const wchar_t wch);

        enum class VTStates
        {
            Ground,
            Escape,
            EscapeIntermediate,
            CsiEntry,
            CsiIntermediate,
            CsiIgnore,
            CsiParam,
            OscParam,
            OscString,
            OscTermination,
            Ss3Entry,
            Ss3Param
        };

        Microsoft::Console::VirtualTerminal::ParserTracing _trace;

        std::unique_ptr<IStateMachineEngine> _pEngine;

        VTStates _state;

        wchar_t _wchIntermediate;
        unsigned short _cIntermediate;

        unsigned short _rgusParams[s_cParamsMax];
        unsigned short _cParams;
        unsigned short* _pusActiveParam;
        unsigned short _iParamAccumulatePos;

        unsigned short _sOscParam;
        unsigned short _sOscNextChar;
        wchar_t _pwchOscStringBuffer[s_cOscStringMaxLength];

        // These members track out state in the parsing of a single string.
        // FlushToTerminal uses these, so that an engine can force a string
        // we're parsing to go straight through to the engine's ActionPassThroughString
        const wchar_t* _pwchCurr;
        const wchar_t* _pwchSequenceStart;
        size_t _currRunLength;

        // This is tracked per state machine instance so that separate calls to Process*
        //   can start and finish a sequence.
        bool _fProcessingIndividually;
    };
}
