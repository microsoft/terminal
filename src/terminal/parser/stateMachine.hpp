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
    // The DEC STD 070 reference recommends supporting up to at least 16384 for
    // parameter values, so 32767 should be more than enough. At most we might
    // want to increase this to 65535, since that is what XTerm and VTE support,
    // but for now 32767 is the safest limit for our existing code base.
    constexpr size_t MAX_PARAMETER_VALUE = 32767;

    // The DEC STD 070 reference requires that a minimum of 16 parameter values
    // are supported, but most modern terminal emulators will allow around twice
    // that number.
    constexpr size_t MAX_PARAMETER_COUNT = 32;

    class StateMachine final
    {
#ifdef UNIT_TESTING
        friend class OutputEngineTest;
        friend class InputEngineTest;
#endif

    public:
        StateMachine(std::unique_ptr<IStateMachineEngine> engine);

        void SetAnsiMode(bool ansiMode) noexcept;

        void ProcessCharacter(const wchar_t wch);
        void ProcessString(const std::wstring_view string);

        void ResetState() noexcept;

        bool FlushToTerminal();

        const IStateMachineEngine& Engine() const noexcept;
        IStateMachineEngine& Engine() noexcept;

    private:
        void _ActionExecute(const wchar_t wch);
        void _ActionExecuteFromEscape(const wchar_t wch);
        void _ActionPrint(const wchar_t wch);
        void _ActionEscDispatch(const wchar_t wch);
        void _ActionVt52EscDispatch(const wchar_t wch);
        void _ActionCollect(const wchar_t wch) noexcept;
        void _ActionParam(const wchar_t wch);
        void _ActionCsiDispatch(const wchar_t wch);
        void _ActionOscParam(const wchar_t wch) noexcept;
        void _ActionOscPut(const wchar_t wch);
        void _ActionOscDispatch(const wchar_t wch);
        void _ActionSs3Dispatch(const wchar_t wch);
        void _ActionDcsDispatch(const wchar_t wch);

        void _ActionClear();
        void _ActionIgnore() noexcept;
        void _ActionInterrupt();

        void _EnterGround() noexcept;
        void _EnterEscape();
        void _EnterEscapeIntermediate() noexcept;
        void _EnterCsiEntry();
        void _EnterCsiParam() noexcept;
        void _EnterCsiIgnore() noexcept;
        void _EnterCsiIntermediate() noexcept;
        void _EnterOscParam() noexcept;
        void _EnterOscString() noexcept;
        void _EnterOscTermination() noexcept;
        void _EnterSs3Entry();
        void _EnterSs3Param() noexcept;
        void _EnterVt52Param() noexcept;
        void _EnterDcsEntry();
        void _EnterDcsParam() noexcept;
        void _EnterDcsIgnore() noexcept;
        void _EnterDcsIntermediate() noexcept;
        void _EnterDcsPassThrough() noexcept;
        void _EnterSosPmApcString() noexcept;

        void _EventGround(const wchar_t wch);
        void _EventEscape(const wchar_t wch);
        void _EventEscapeIntermediate(const wchar_t wch);
        void _EventCsiEntry(const wchar_t wch);
        void _EventCsiIntermediate(const wchar_t wch);
        void _EventCsiIgnore(const wchar_t wch);
        void _EventCsiParam(const wchar_t wch);
        void _EventOscParam(const wchar_t wch) noexcept;
        void _EventOscString(const wchar_t wch);
        void _EventOscTermination(const wchar_t wch);
        void _EventSs3Entry(const wchar_t wch);
        void _EventSs3Param(const wchar_t wch);
        void _EventVt52Param(const wchar_t wch);
        void _EventDcsEntry(const wchar_t wch);
        void _EventDcsIgnore() noexcept;
        void _EventDcsIntermediate(const wchar_t wch);
        void _EventDcsParam(const wchar_t wch);
        void _EventDcsPassThrough(const wchar_t wch);
        void _EventSosPmApcString(const wchar_t wch) noexcept;

        void _AccumulateTo(const wchar_t wch, size_t& value) noexcept;

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
            Ss3Param,
            Vt52Param,
            DcsEntry,
            DcsIgnore,
            DcsIntermediate,
            DcsParam,
            DcsPassThrough,
            SosPmApcString
        };

        Microsoft::Console::VirtualTerminal::ParserTracing _trace;

        std::unique_ptr<IStateMachineEngine> _engine;

        VTStates _state;

        bool _isInAnsiMode;

        std::wstring_view _run;

        VTIDBuilder _identifier;
        std::vector<VTParameter> _parameters;
        bool _parameterLimitReached;

        std::wstring _oscString;
        size_t _oscParameter;

        IStateMachineEngine::StringHandler _dcsStringHandler;

        std::optional<std::wstring> _cachedSequence;

        // This is tracked per state machine instance so that separate calls to Process*
        //   can start and finish a sequence.
        bool _processingIndividually;
    };
}
