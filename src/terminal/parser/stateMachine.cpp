// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"

#include "ascii.hpp"

using namespace Microsoft::Console::VirtualTerminal;

//Takes ownership of the pEngine.
StateMachine::StateMachine(std::unique_ptr<IStateMachineEngine> engine) :
    _engine(std::move(engine)),
    _state(VTStates::Ground),
    _trace(Microsoft::Console::VirtualTerminal::ParserTracing()),
    _isInAnsiMode(true),
    _parameters{},
    _parameterLimitReached(false),
    _oscString{},
    _cachedSequence{ std::nullopt },
    _processingIndividually(false)
{
    _ActionClear();
}

void StateMachine::SetAnsiMode(bool ansiMode) noexcept
{
    _isInAnsiMode = ansiMode;
}

const IStateMachineEngine& StateMachine::Engine() const noexcept
{
    return *_engine;
}

IStateMachineEngine& StateMachine::Engine() noexcept
{
    return *_engine;
}

// Routine Description:
// - Determines if a character is a valid number character, 0-9.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isNumericParamValue(const wchar_t wch) noexcept
{
    return wch >= L'0' && wch <= L'9'; // 0x30 - 0x39
}

#pragma warning(push)
#pragma warning(disable : 26497) // We don't use any of these "constexprable" functions in that fashion

// Routine Description:
// - Determines if a character belongs to the C0 escape range.
//   This is character sequences less than a space character (null, backspace, new line, etc.)
//   See also https://en.wikipedia.org/wiki/C0_and_C1_control_codes
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isC0Code(const wchar_t wch) noexcept
{
    return (wch >= AsciiChars::NUL && wch <= AsciiChars::ETB) ||
           wch == AsciiChars::EM ||
           (wch >= AsciiChars::FS && wch <= AsciiChars::US);
}

// Routine Description:
// - Determines if a character is a C1 control characters.
//   This is a single-character way to start a control sequence, as opposed to using ESC
//   and their 7-bit equivalent.
//
//   Not all single-byte codepages support C1 control codes--in some, the range that would
//   be used for C1 codes are instead used for additional graphic characters.
//
//   However, we do not need to worry about confusion whether a single byte, for example,
//   \x9b in a single-byte stream represents a C1 CSI or some other glyph, because by the time we
//   get here, everything is Unicode. Knowing whether a single-byte \x9b represents a
//   single-character C1 CSI or some other glyph is handled by MultiByteToWideChar before
//   we get here (if the stream was not already UTF-16). For instance, in CP_ACP, if a
//   \x9b shows up, it will get converted to \x203a. So, if we get here, and have a
//   \x009b, we know that it unambiguously represents a C1 CSI.
//
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isC1ControlCharacter(const wchar_t wch) noexcept
{
    return (wch >= L'\x80' && wch <= L'\x9F');
}

// Routine Description:
// - Convert a C1 control characters to their 7-bit equivalent.
//
// Arguments:
// - wch - Character to convert.
// Return Value:
// - The 7-bit equivalent of the 8-bit control characters.
static constexpr wchar_t _c1To7Bit(const wchar_t wch) noexcept
{
    return wch - L'\x40';
}

// Routine Description:
// - Determines if a character is a valid intermediate in an VT escape sequence.
//   Intermediates are punctuation type characters that are generally vendor specific and
//   modify the operational mode of a command.
//   See also http://vt100.net/emu/dec_ansi_parser
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isIntermediate(const wchar_t wch) noexcept
{
    return wch >= L' ' && wch <= L'/'; // 0x20 - 0x2F
}

// Routine Description:
// - Determines if a character is the delete character.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isDelete(const wchar_t wch) noexcept
{
    return wch == AsciiChars::DEL;
}

// Routine Description:
// - Determines if a character is the escape character.
//   Used to start escape sequences.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isEscape(const wchar_t wch) noexcept
{
    return wch == AsciiChars::ESC;
}

// Routine Description:
// - Determines if a character is a delimiter between two parameters in an escape sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isParameterDelimiter(const wchar_t wch) noexcept
{
    return wch == L';'; // 0x3B
}

// Routine Description:
// - Determines if a character is "control sequence" beginning indicator.
//   This immediately follows an escape and signifies a varying length control sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isCsiIndicator(const wchar_t wch) noexcept
{
    return wch == L'['; // 0x5B
}

// Routine Description:
// - Determines if a character is a private range marker for a control sequence.
//   Private range markers indicate vendor-specific behavior.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isCsiPrivateMarker(const wchar_t wch) noexcept
{
    return wch == L'<' || wch == L'=' || wch == L'>' || wch == L'?'; // 0x3C - 0x3F
}

// Routine Description:
// - Determines if a character is invalid in a control sequence
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isCsiInvalid(const wchar_t wch) noexcept
{
    return wch == L':'; // 0x3A
}

// Routine Description:
// - Determines if a character is an invalid intermediate.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isIntermediateInvalid(const wchar_t wch) noexcept
{
    // 0x30 - 0x3F
    return _isNumericParamValue(wch) || _isCsiInvalid(wch) || _isParameterDelimiter(wch) || _isCsiPrivateMarker(wch);
}

// Routine Description:
// - Determines if a character is an invalid parameter.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isParameterInvalid(const wchar_t wch) noexcept
{
    // 0x3A, 0x3C - 0x3F
    return _isCsiInvalid(wch) || _isCsiPrivateMarker(wch);
}

// Routine Description:
// - Determines if a character is a string terminator indicator.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isStringTerminatorIndicator(const wchar_t wch) noexcept
{
    return wch == L'\\'; // 0x5c
}

// Routine Description:
// - Determines if a character is a "Single Shift Select" indicator.
//   This immediately follows an escape and signifies a varying length control string.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isSs3Indicator(const wchar_t wch) noexcept
{
    return wch == L'O'; // 0x4F
}

// Routine Description:
// - Determines if a character is the VT52 "Direct Cursor Address" command.
//   This immediately follows an escape and signifies the start of a multiple
//      character command sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isVt52CursorAddress(const wchar_t wch) noexcept
{
    return wch == L'Y'; // 0x59
}

// Routine Description:
// - Determines if a character is "operating system control string" beginning
//      indicator.
//   This immediately follows an escape and signifies a  signifies a varying
//      length control sequence, quite similar to CSI.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isOscIndicator(const wchar_t wch) noexcept
{
    return wch == L']'; // 0x5D
}

// Routine Description:
// - Determines if a character is a delimiter between two parameters in a "operating system control sequence"
//   This occurs in the middle of a control sequence after escape and OscIndicator have been recognized,
//   after the parameter indicating which OSC action to take.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isOscDelimiter(const wchar_t wch) noexcept
{
    return wch == L';'; // 0x3B
}

// Routine Description:
// - Determines if a character should be ignored in a operating system control sequence
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isOscInvalid(const wchar_t wch) noexcept
{
    return wch <= L'\x17' ||
           wch == L'\x19' ||
           (wch >= L'\x1c' && wch <= L'\x1f');
}

// Routine Description:
// - Determines if a character is "operating system control string" termination indicator.
//   This signals the end of an OSC string collection.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isOscTerminator(const wchar_t wch) noexcept
{
    return wch == AsciiChars::BEL; // Bell character
}

// Routine Description:
// - Determines if a character is "device control string" beginning
//      indicator.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isDcsIndicator(const wchar_t wch) noexcept
{
    return wch == L'P'; // 0x50
}

// Routine Description:
// - Determines if a character is valid for a DCS pass through sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isDcsPassThroughValid(const wchar_t wch) noexcept
{
    // 0x20 - 0x7E
    return wch >= AsciiChars::SPC && wch < AsciiChars::DEL;
}

// Routine Description:
// - Determines if a character is "start of string" beginning
//      indicator.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isSosIndicator(const wchar_t wch) noexcept
{
    return wch == L'X'; // 0x58
}

// Routine Description:
// - Determines if a character is "private message" beginning
//      indicator.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isPmIndicator(const wchar_t wch) noexcept
{
    return wch == L'^'; // 0x5E
}

// Routine Description:
// - Determines if a character is "application program command" beginning
//      indicator.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isApcIndicator(const wchar_t wch) noexcept
{
    return wch == L'_'; // 0x5F
}

// Routine Description:
// - Determines if a character indicates an action that should be taken in the ground state -
//     These are C0 characters and the C1 [single-character] CSI.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
static constexpr bool _isActionableFromGround(const wchar_t wch) noexcept
{
    return (wch <= AsciiChars::US) || _isC1ControlCharacter(wch) || _isDelete(wch);
}

#pragma warning(pop)

// Routine Description:
// - Triggers the Execute action to indicate that the listener should immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionExecute(const wchar_t wch)
{
    _trace.TraceOnExecute(wch);
    const bool success = _engine->ActionExecute(wch);

    // Trace the result.
    _trace.DispatchSequenceTrace(success);
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character, with the added
//      information that we're executing it from the Escape state.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionExecuteFromEscape(const wchar_t wch)
{
    _trace.TraceOnExecuteFromEscape(wch);

    const bool success = _engine->ActionExecuteFromEscape(wch);

    // Trace the result.
    _trace.DispatchSequenceTrace(success);
}

// Routine Description:
// - Triggers the Print action to indicate that the listener should render the character given.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionPrint(const wchar_t wch)
{
    _trace.TraceOnAction(L"Print");

    const bool success = _engine->ActionPrint(wch);

    // Trace the result.
    _trace.DispatchSequenceTrace(success);
}

// Routine Description:
// - Triggers the EscDispatch action to indicate that the listener should handle a simple escape sequence.
//   These sequences traditionally start with ESC and a simple letter. No complicated parameters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionEscDispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"EscDispatch");

    const bool success = _engine->ActionEscDispatch(_identifier.Finalize(wch));

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (!success)
    {
        // Suppress it and log telemetry on failed cases
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Triggers the Vt52EscDispatch action to indicate that the listener should handle a VT52 escape sequence.
//   These sequences start with ESC and a single letter, sometimes followed by parameters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionVt52EscDispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"Vt52EscDispatch");

    const bool success = _engine->ActionVt52EscDispatch(_identifier.Finalize(wch),
                                                        { _parameters.data(), _parameters.size() });

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (!success)
    {
        // Suppress it and log telemetry on failed cases
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionCsiDispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"CsiDispatch");

    const bool success = _engine->ActionCsiDispatch(_identifier.Finalize(wch),
                                                    { _parameters.data(), _parameters.size() });

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (!success)
    {
        // Suppress it and log telemetry on failed cases
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Triggers the Collect action to indicate that the state machine should store this character as part of an escape/control sequence.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionCollect(const wchar_t wch) noexcept
{
    _trace.TraceOnAction(L"Collect");

    // store collect data
    _identifier.AddIntermediate(wch);
}

// Routine Description:
// - Triggers the Param action to indicate that the state machine should store this character as a part of a parameter
//   to a control sequence.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionParam(const wchar_t wch)
{
    _trace.TraceOnAction(L"Param");

    // Once we've reached the parameter limit, additional parameters are ignored.
    if (!_parameterLimitReached)
    {
        // If we have no parameters and we're about to add one, get the next value ready here.
        if (_parameters.empty())
        {
            _parameters.push_back({});
        }

        // On a delimiter, increase the number of params we've seen.
        // "Empty" params should still count as a param -
        //      eg "\x1b[0;;m" should be three params
        if (_isParameterDelimiter(wch))
        {
            // If we receive a delimiter after we've already accumulated the
            // maximum allowed parameters, then we need to set a flag to
            // indicate that further parameter characters should be ignored.
            if (_parameters.size() >= MAX_PARAMETER_COUNT)
            {
                _parameterLimitReached = true;
            }
            else
            {
                // Otherwise move to next param.
                _parameters.push_back({});
            }
        }
        else
        {
            // Accumulate the character given into the last (current) parameter.
            // If the value hasn't been initialized yet, it'll start as 0.
            auto currentParameter = _parameters.back().value_or(0);
            _AccumulateTo(wch, currentParameter);
            _parameters.back() = currentParameter;
        }
    }
}

// Routine Description:
// - Triggers the Clear action to indicate that the state machine should erase all internal state.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionClear()
{
    _trace.TraceOnAction(L"Clear");

    // clear all internal stored state.
    _identifier.Clear();

    _parameters.clear();
    _parameterLimitReached = false;

    _oscString.clear();
    _oscParameter = 0;

    _dcsStringHandler = nullptr;

    _engine->ActionClear();
}

// Routine Description:
// - Triggers the Ignore action to indicate that the state machine should eat this character and say nothing.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionIgnore() noexcept
{
    // do nothing.
    _trace.TraceOnAction(L"Ignore");
}

// Routine Description:
// - Triggers the end of a data string when a CAN, SUB, or ESC is seen.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_ActionInterrupt()
{
    // This is only applicable for DCS strings. OSC strings require a full
    // ST sequence to be received before they can be dispatched.
    if (_state == VTStates::DcsPassThrough)
    {
        // The ESC signals the end of the data string.
        _dcsStringHandler(AsciiChars::ESC);
        _dcsStringHandler = nullptr;
    }
}

// Routine Description:
// - Stores this character as part of the param indicating which OSC action to take.
// Arguments:
// - wch - Character to collect.
// Return Value:
// - <none>
void StateMachine::_ActionOscParam(const wchar_t wch) noexcept
{
    _trace.TraceOnAction(L"OscParamCollect");

    _AccumulateTo(wch, _oscParameter);
}

// Routine Description:
// - Stores this character as part of the OSC string
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionOscPut(const wchar_t wch)
{
    _trace.TraceOnAction(L"OscPut");

    _oscString.push_back(wch);
}

// Routine Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionOscDispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"OscDispatch");

    const bool success = _engine->ActionOscDispatch(wch, _oscParameter, _oscString);

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (!success)
    {
        // Suppress it and log telemetry on failed cases
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionSs3Dispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"Ss3Dispatch");

    const bool success = _engine->ActionSs3Dispatch(wch, { _parameters.data(), _parameters.size() });

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (!success)
    {
        // Suppress it and log telemetry on failed cases
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Triggers the DcsDispatch action to indicate that the listener should handle a control sequence.
//   The returned handler function will be used to process the subsequent data string characters.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionDcsDispatch(const wchar_t wch)
{
    _trace.TraceOnAction(L"DcsDispatch");

    _dcsStringHandler = _engine->ActionDcsDispatch(_identifier.Finalize(wch),
                                                   { _parameters.data(), _parameters.size() });

    // If the returned handler is null, the sequence is not supported.
    const bool success = _dcsStringHandler != nullptr;

    // Trace the result.
    _trace.DispatchSequenceTrace(success);

    if (success)
    {
        // If successful, enter the pass through state.
        _EnterDcsPassThrough();
    }
    else
    {
        // Otherwise ignore remaining chars and log telemetry on failed cases
        _EnterDcsIgnore();
        TermTelemetry::Instance().LogFailed(wch);
    }
}

// Routine Description:
// - Moves the state machine into the Ground state.
//   This state is entered:
//   1. By default at the beginning of operation
//   2. After any execute/dispatch action.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterGround() noexcept
{
    _state = VTStates::Ground;
    _cachedSequence.reset(); // entering ground means we've completed the pending sequence
    _trace.TraceStateChange(L"Ground");
}

// Routine Description:
// - Moves the state machine into the Escape state.
//   This state is entered:
//   1. When the Escape character is seen at any time.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterEscape()
{
    _state = VTStates::Escape;
    _trace.TraceStateChange(L"Escape");
    _ActionClear();
    _trace.ClearSequenceTrace();
}

// Routine Description:
// - Moves the state machine into the EscapeIntermediate state.
//   This state is entered:
//   1. When EscIntermediate characters are seen after an Escape entry (only from the Escape state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterEscapeIntermediate() noexcept
{
    _state = VTStates::EscapeIntermediate;
    _trace.TraceStateChange(L"EscapeIntermediate");
}

// Routine Description:
// - Moves the state machine into the CsiEntry state.
//   This state is entered:
//   1. When the CsiEntry character is seen after an Escape entry (only from the Escape state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterCsiEntry()
{
    _state = VTStates::CsiEntry;
    _trace.TraceStateChange(L"CsiEntry");
    _ActionClear();
}

// Routine Description:
// - Moves the state machine into the CsiParam state.
//   This state is entered:
//   1. When valid parameter characters are detected on entering a CSI (from CsiEntry state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterCsiParam() noexcept
{
    _state = VTStates::CsiParam;
    _trace.TraceStateChange(L"CsiParam");
}

// Routine Description:
// - Moves the state machine into the CsiIgnore state.
//   This state is entered:
//   1. When an invalid character is detected during a CSI sequence indicating we should ignore the whole sequence.
//      (From CsiEntry, CsiParam, or CsiIntermediate states.)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterCsiIgnore() noexcept
{
    _state = VTStates::CsiIgnore;
    _trace.TraceStateChange(L"CsiIgnore");
}

// Routine Description:
// - Moves the state machine into the CsiIntermediate state.
//   This state is entered:
//   1. When an intermediate character is seen immediately after entering a control sequence (from CsiEntry)
//   2. When an intermediate character is seen while collecting parameter data (from CsiParam)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterCsiIntermediate() noexcept
{
    _state = VTStates::CsiIntermediate;
    _trace.TraceStateChange(L"CsiIntermediate");
}

// Routine Description:
// - Moves the state machine into the OscParam state.
//   This state is entered:
//   1. When an OscEntry character (']') is seen after an Escape entry (only from the Escape state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterOscParam() noexcept
{
    _state = VTStates::OscParam;
    _trace.TraceStateChange(L"OscParam");
}

// Routine Description:
// - Moves the state machine into the OscString state.
//   This state is entered:
//   1. When a delimiter character (';') is seen in the OSC Param state.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterOscString() noexcept
{
    _state = VTStates::OscString;
    _trace.TraceStateChange(L"OscString");
}

// Routine Description:
// - Moves the state machine into the OscTermination state.
//   This state is entered:
//   1. When an ESC is seen in an OSC string. This escape will be followed by a
//      '\', as to encode a 0x9C as a 7-bit ASCII char stream.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterOscTermination() noexcept
{
    _state = VTStates::OscTermination;
    _trace.TraceStateChange(L"OscTermination");
}

// Routine Description:
// - Moves the state machine into the Ss3Entry state.
//   This state is entered:
//   1. When the Ss3Entry character is seen after an Escape entry (only from the Escape state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterSs3Entry()
{
    _state = VTStates::Ss3Entry;
    _trace.TraceStateChange(L"Ss3Entry");
    _ActionClear();
}

// Routine Description:
// - Moves the state machine into the Ss3Param state.
//   This state is entered:
//   1. When valid parameter characters are detected on entering a SS3 (from Ss3Entry state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterSs3Param() noexcept
{
    _state = VTStates::Ss3Param;
    _trace.TraceStateChange(L"Ss3Param");
}

// Routine Description:
// - Moves the state machine into the VT52Param state.
//   This state is entered:
//   1. When a VT52 Cursor Address escape is detected, so parameters are expected to follow.
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterVt52Param() noexcept
{
    _state = VTStates::Vt52Param;
    _trace.TraceStateChange(L"Vt52Param");
}

// Routine Description:
// - Moves the state machine into the DcsEntry state.
//   This state is entered:
//   1. When the DcsEntry character is seen after an Escape entry (only from the Escape state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterDcsEntry()
{
    _state = VTStates::DcsEntry;
    _trace.TraceStateChange(L"DcsEntry");
    _ActionClear();
}

// Routine Description:
// - Moves the state machine into the DcsParam state.
//   This state is entered:
//   1. When valid parameter characters are detected on entering a DCS (from DcsEntry state)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterDcsParam() noexcept
{
    _state = VTStates::DcsParam;
    _trace.TraceStateChange(L"DcsParam");
}

// Routine Description:
// - Moves the state machine into the DcsIgnore state.
//   This state is entered:
//   1. When an invalid character is detected during a DCS sequence indicating we should ignore the whole sequence.
//      (From DcsEntry, DcsParam, DcsPassThrough, or DcsIntermediate states.)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterDcsIgnore() noexcept
{
    _state = VTStates::DcsIgnore;
    _trace.TraceStateChange(L"DcsIgnore");
}

// Routine Description:
// - Moves the state machine into the DcsIntermediate state.
//   This state is entered:
//   1. When an intermediate character is seen immediately after entering a control sequence (from DcsEntry)
//   2. When an intermediate character is seen while collecting parameter data (from DcsParam)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterDcsIntermediate() noexcept
{
    _state = VTStates::DcsIntermediate;
    _trace.TraceStateChange(L"DcsIntermediate");
}

// Routine Description:
// - Moves the state machine into the DcsPassThrough state.
//   This state is entered:
//   1. When a data string character is seen immediately after entering a control sequence (from DcsEntry)
//   2. When a data string character is seen while collecting parameter data (from DcsParam)
//   3. When a data string character is seen while collecting intermediate data (from DcsIntermediate)
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterDcsPassThrough() noexcept
{
    _state = VTStates::DcsPassThrough;
    _trace.TraceStateChange(L"DcsPassThrough");
}

// Routine Description:
// - Moves the state machine into the SosPmApcString state.
//   This state is entered:
//   1. When the Sos character is seen after an Escape entry
//   2. When the Pm character is seen after an Escape entry
//   3. When the Apc character is seen after an Escape entry
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::_EnterSosPmApcString() noexcept
{
    _state = VTStates::SosPmApcString;
    _trace.TraceStateChange(L"SosPmApcString");
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the Ground state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Print all other characters
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventGround(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Ground");
    if (_isC0Code(wch) || _isDelete(wch))
    {
        _ActionExecute(wch);
    }
    else
    {
        _ActionPrint(wch);
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the Escape state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Enter Control Sequence state
//   5. Dispatch an Escape action.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventEscape(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Escape");
    if (_isC0Code(wch))
    {
        if (_engine->DispatchControlCharsFromEscape())
        {
            _ActionExecuteFromEscape(wch);
            _EnterGround();
        }
        else
        {
            _ActionExecute(wch);
        }
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediate(wch))
    {
        if (_engine->DispatchIntermediatesFromEscape())
        {
            _ActionEscDispatch(wch);
            _EnterGround();
        }
        else
        {
            _ActionCollect(wch);
            _EnterEscapeIntermediate();
        }
    }
    else if (_isInAnsiMode)
    {
        if (_isCsiIndicator(wch))
        {
            _EnterCsiEntry();
        }
        else if (_isOscIndicator(wch))
        {
            _EnterOscParam();
        }
        else if (_isSs3Indicator(wch) && _engine->ParseControlSequenceAfterSs3())
        {
            _EnterSs3Entry();
        }
        else if (_isDcsIndicator(wch))
        {
            _EnterDcsEntry();
        }
        else if (_isSosIndicator(wch) || _isPmIndicator(wch) || _isApcIndicator(wch))
        {
            _EnterSosPmApcString();
        }
        else
        {
            _ActionEscDispatch(wch);
            _EnterGround();
        }
    }
    else if (_isVt52CursorAddress(wch))
    {
        _EnterVt52Param();
    }
    else
    {
        _ActionVt52EscDispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the EscapeIntermediate state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Dispatch an Escape action.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventEscapeIntermediate(const wchar_t wch)
{
    _trace.TraceOnEvent(L"EscapeIntermediate");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isInAnsiMode)
    {
        _ActionEscDispatch(wch);
        _EnterGround();
    }
    else if (_isVt52CursorAddress(wch))
    {
        _EnterVt52Param();
    }
    else
    {
        _ActionVt52EscDispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the CsiEntry state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   5. Store parameter data
//   6. Collect Control Sequence Private markers
//   7. Dispatch a control sequence with parameters for action
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventCsiEntry(const wchar_t wch)
{
    _trace.TraceOnEvent(L"CsiEntry");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterCsiIntermediate();
    }
    else if (_isCsiInvalid(wch))
    {
        _EnterCsiIgnore();
    }
    else if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
        _EnterCsiParam();
    }
    else if (_isCsiPrivateMarker(wch))
    {
        _ActionCollect(wch);
        _EnterCsiParam();
    }
    else
    {
        _ActionCsiDispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the CsiIntermediate state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   5. Dispatch a control sequence with parameters for action
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventCsiIntermediate(const wchar_t wch)
{
    _trace.TraceOnEvent(L"CsiIntermediate");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediateInvalid(wch))
    {
        _EnterCsiIgnore();
    }
    else
    {
        _ActionCsiDispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the CsiIgnore state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   5. Return to Ground
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventCsiIgnore(const wchar_t wch)
{
    _trace.TraceOnEvent(L"CsiIgnore");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediate(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediateInvalid(wch))
    {
        _ActionIgnore();
    }
    else
    {
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the CsiParam state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Collect Intermediate characters
//   4. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   5. Store parameter data
//   6. Dispatch a control sequence with parameters for action
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventCsiParam(const wchar_t wch)
{
    _trace.TraceOnEvent(L"CsiParam");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterCsiIntermediate();
    }
    else if (_isParameterInvalid(wch))
    {
        _EnterCsiIgnore();
    }
    else
    {
        _ActionCsiDispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the OscParam state.
//   Events in this state will:
//   1. Collect numeric values into an Osc Param
//   2. Move to the OscString state on a delimiter
//   3. Ignore everything else.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventOscParam(const wchar_t wch) noexcept
{
    _trace.TraceOnEvent(L"OscParam");
    if (_isOscTerminator(wch))
    {
        _EnterGround();
    }
    else if (_isNumericParamValue(wch))
    {
        _ActionOscParam(wch);
    }
    else if (_isOscDelimiter(wch))
    {
        _EnterOscString();
    }
    else
    {
        _ActionIgnore();
    }
}

// Routine Description:
// - Processes a character event into a Action that occurs while in the OscParam state.
//   Events in this state will:
//   1. Trigger the OSC action associated with the param on an OscTerminator
//   2. If we see a ESC, enter the OscTermination state. We'll wait for one
//      more character before we dispatch the string.
//   3. Ignore OscInvalid characters.
//   4. Collect everything else into the OscString
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventOscString(const wchar_t wch)
{
    _trace.TraceOnEvent(L"OscString");
    if (_isOscTerminator(wch))
    {
        _ActionOscDispatch(wch);
        _EnterGround();
    }
    else if (_isEscape(wch))
    {
        _EnterOscTermination();
    }
    else if (_isOscInvalid(wch))
    {
        _ActionIgnore();
    }
    else
    {
        // add this character to our OSC string
        _ActionOscPut(wch);
    }
}

// Routine Description:
// - Handle the two-character termination of a OSC sequence.
//   Events in this state will:
//   1. Trigger the OSC action associated with the param on an OscTerminator
//   2. Otherwise treat this as a normal escape character event.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventOscTermination(const wchar_t wch)
{
    _trace.TraceOnEvent(L"OscTermination");
    if (_isStringTerminatorIndicator(wch))
    {
        _ActionOscDispatch(wch);
        _EnterGround();
    }
    else
    {
        _EnterEscape();
        _EventEscape(wch);
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the Ss3Entry state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   4. Store parameter data
//   5. Dispatch a control sequence with parameters for action
//  SS3 sequences are structurally the same as CSI sequences, just with a
//      different initiation. It's safe to reuse CSI's functions for
//      determining if a character is a parameter, delimiter, or invalid.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventSs3Entry(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Ss3Entry");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isCsiInvalid(wch))
    {
        // It's safe for us to go into the CSI ignore here, because both SS3 and
        //      CSI sequences ignore characters the same way.
        _EnterCsiIgnore();
    }
    else if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
        _EnterSs3Param();
    }
    else
    {
        _ActionSs3Dispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the CsiParam state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Begin to ignore all remaining parameters when an invalid character is detected (CsiIgnore)
//   4. Store parameter data
//   5. Dispatch a control sequence with parameters for action
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventSs3Param(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Ss3Param");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
    }
    else if (_isParameterInvalid(wch))
    {
        _EnterCsiIgnore();
    }
    else
    {
        _ActionSs3Dispatch(wch);
        _EnterGround();
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the Vt52Param state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Ignore Delete characters
//   3. Store exactly two parameter characters
//   4. Dispatch a control sequence with parameters for action (always Direct Cursor Address)
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventVt52Param(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Vt52Param");
    if (_isC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else
    {
        _parameters.push_back(wch);
        if (_parameters.size() == 2)
        {
            // The command character is processed before the parameter values,
            // but it will always be 'Y', the Direct Cursor Address command.
            _ActionVt52EscDispatch(L'Y');
            _EnterGround();
        }
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the DcsEntry state.
//   Events in this state will:
//   1. Ignore C0 control characters
//   2. Ignore Delete characters
//   3. Begin to ignore all remaining characters when an invalid character is detected (DcsIgnore)
//   4. Store parameter data
//   5. Collect Intermediate characters
//   6. Dispatch the Final character in preparation for parsing the data string
//  DCS sequences are structurally almost the same as CSI sequences, just with an
//      extra data string. It's safe to reuse CSI functions for
//      determining if a character is a parameter, delimiter, or invalid.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventDcsEntry(const wchar_t wch)
{
    _trace.TraceOnEvent(L"DcsEntry");
    if (_isC0Code(wch))
    {
        _ActionIgnore();
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isCsiInvalid(wch))
    {
        _EnterDcsIgnore();
    }
    else if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
        _EnterDcsParam();
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterDcsIntermediate();
    }
    else
    {
        _ActionDcsDispatch(wch);
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the DcsIgnore state.
//   In this state the entire DCS string is considered invalid and we will ignore everything.
//   The termination state is handled outside when an ESC is seen.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventDcsIgnore() noexcept
{
    _trace.TraceOnEvent(L"DcsIgnore");
    _ActionIgnore();
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the DcsIntermediate state.
//   Events in this state will:
//   1. Ignore C0 control characters
//   2. Ignore Delete characters
//   3. Collect intermediate data.
//   4. Begin to ignore all remaining intermediates when an invalid character is detected (DcsIgnore)
//   5. Dispatch the Final character in preparation for parsing the data string
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventDcsIntermediate(const wchar_t wch)
{
    _trace.TraceOnEvent(L"DcsIntermediate");
    if (_isC0Code(wch))
    {
        _ActionIgnore();
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
    }
    else if (_isIntermediateInvalid(wch))
    {
        _EnterDcsIgnore();
    }
    else
    {
        _ActionDcsDispatch(wch);
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the DcsParam state.
//   Events in this state will:
//   1. Ignore C0 control characters
//   2. Ignore Delete characters
//   3. Collect DCS parameter data
//   4. Enter DcsIntermediate if we see an intermediate
//   5. Begin to ignore all remaining parameters when an invalid character is detected (DcsIgnore)
//   6. Dispatch the Final character in preparation for parsing the data string
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventDcsParam(const wchar_t wch)
{
    _trace.TraceOnEvent(L"DcsParam");
    if (_isC0Code(wch))
    {
        _ActionIgnore();
    }
    else if (_isDelete(wch))
    {
        _ActionIgnore();
    }
    if (_isNumericParamValue(wch) || _isParameterDelimiter(wch))
    {
        _ActionParam(wch);
    }
    else if (_isIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterDcsIntermediate();
    }
    else if (_isParameterInvalid(wch))
    {
        _EnterDcsIgnore();
    }
    else
    {
        _ActionDcsDispatch(wch);
    }
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the DcsPassThrough state.
//   Events in this state will:
//   1. Pass through if character is valid.
//   2. Ignore everything else.
//   The termination state is handled outside when an ESC is seen.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventDcsPassThrough(const wchar_t wch)
{
    _trace.TraceOnEvent(L"DcsPassThrough");
    if (_isC0Code(wch) || _isDcsPassThroughValid(wch))
    {
        if (!_dcsStringHandler(wch))
        {
            _EnterDcsIgnore();
        }
    }
    else
    {
        _ActionIgnore();
    }
}

// Routine Description:
// - Handle SOS/PM/APC string.
//   In this state the entire string is ignored.
//   The termination state is handled outside when an ESC is seen.
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventSosPmApcString(const wchar_t /*wch*/) noexcept
{
    _trace.TraceOnEvent(L"SosPmApcString");
    _ActionIgnore();
}

// Routine Description:
// - Entry to the state machine. Takes characters one by one and processes them according to the state machine rules.
// Arguments:
// - wch - New character to operate upon
// Return Value:
// - <none>
void StateMachine::ProcessCharacter(const wchar_t wch)
{
    _trace.TraceCharInput(wch);

    // Process "from anywhere" events first.
    const bool isFromAnywhereChar = (wch == AsciiChars::CAN || wch == AsciiChars::SUB);

    // GH#4201 - If this sequence was ^[^X or ^[^Z, then we should
    // _ActionExecuteFromEscape, as to send a Ctrl+Alt+key key. We should only
    // do this for the InputStateMachineEngine - the OutputEngine should execute
    // these from any state.
    if (isFromAnywhereChar && !(_state == VTStates::Escape && _engine->DispatchControlCharsFromEscape()))
    {
        _ActionInterrupt();
        _ActionExecute(wch);
        _EnterGround();
    }
    // Preprocess C1 control characters and treat them as ESC + their 7-bit equivalent.
    else if (_isC1ControlCharacter(wch))
    {
        ProcessCharacter(AsciiChars::ESC);
        ProcessCharacter(_c1To7Bit(wch));
    }
    // Don't go to escape from the OSC string state - ESC can be used to terminate OSC strings.
    else if (_isEscape(wch) && _state != VTStates::OscString)
    {
        _ActionInterrupt();
        _EnterEscape();
    }
    else
    {
        // Then pass to the current state as an event
        switch (_state)
        {
        case VTStates::Ground:
            return _EventGround(wch);
        case VTStates::Escape:
            return _EventEscape(wch);
        case VTStates::EscapeIntermediate:
            return _EventEscapeIntermediate(wch);
        case VTStates::CsiEntry:
            return _EventCsiEntry(wch);
        case VTStates::CsiIntermediate:
            return _EventCsiIntermediate(wch);
        case VTStates::CsiIgnore:
            return _EventCsiIgnore(wch);
        case VTStates::CsiParam:
            return _EventCsiParam(wch);
        case VTStates::OscParam:
            return _EventOscParam(wch);
        case VTStates::OscString:
            return _EventOscString(wch);
        case VTStates::OscTermination:
            return _EventOscTermination(wch);
        case VTStates::Ss3Entry:
            return _EventSs3Entry(wch);
        case VTStates::Ss3Param:
            return _EventSs3Param(wch);
        case VTStates::Vt52Param:
            return _EventVt52Param(wch);
        case VTStates::DcsEntry:
            return _EventDcsEntry(wch);
        case VTStates::DcsIgnore:
            return _EventDcsIgnore();
        case VTStates::DcsIntermediate:
            return _EventDcsIntermediate(wch);
        case VTStates::DcsParam:
            return _EventDcsParam(wch);
        case VTStates::DcsPassThrough:
            return _EventDcsPassThrough(wch);
        case VTStates::SosPmApcString:
            return _EventSosPmApcString(wch);
        default:
            return;
        }
    }
}
// Method Description:
// - Pass the current string we're processing through to the engine. It may eat
//      the string, it may write it straight to the input unmodified, it might
//      write the string to the tty application. A pointer to this function will
//      get handed to the OutputStateMachineEngine, so that it can write strings
//      it doesn't understand to the tty.
//  This does not modify the state of the state machine. Callers should be in
//      the Action*Dispatch state, and upon completion, the state's handler (eg
//      _EventCsiParam) should move us into the ground state.
// Arguments:
// - <none>
// Return Value:
// - true if the engine successfully handled the string.
bool StateMachine::FlushToTerminal()
{
    bool success{ true };

    if (success && _cachedSequence.has_value())
    {
        // Flush the partial sequence to the terminal before we flush the rest of it.
        // We always want to clear the sequence, even if we failed, so we don't accumulate bad state
        // and dump it out elsewhere later.
        success = _engine->ActionPassThroughString(*_cachedSequence);
        _cachedSequence.reset();
    }

    if (success)
    {
        // _pwchCurr is incremented after a call to ProcessCharacter to indicate
        //      that pwchCurr was processed.
        // However, if we're here, then the processing of pwchChar triggered the
        //      engine to request the entire sequence get passed through, including pwchCurr.
        success = _engine->ActionPassThroughString(_run);
    }

    return success;
}

// Routine Description:
// - Helper for entry to the state machine. Will take an array of characters
//     and print as many as it can without encountering a character indicating
//     a escape sequence, then feed characters into the state machine one at a
//     time until we return to the ground state.
// Arguments:
// - string - Characters to operate upon
// Return Value:
// - <none>
void StateMachine::ProcessString(const std::wstring_view string)
{
    size_t start = 0;
    size_t current = start;

    while (current < string.size())
    {
        // The run will be everything from the start INCLUDING the current one
        // in case we process the current character and it turns into a passthrough
        // fallback that picks up this _run inside `FlushToTerminal` above.
        _run = string.substr(start, current - start + 1);

        if (_processingIndividually)
        {
            // If we're processing characters individually, send it to the state machine.
            ProcessCharacter(string.at(current));
            ++current;
            if (_state == VTStates::Ground) // Then check if we're back at ground. If we are, the next character (pwchCurr)
            { //   is the start of the next run of characters that might be printable.
                _processingIndividually = false;
                start = current;
            }
        }
        else
        {
            if (_isActionableFromGround(string.at(current))) // If the current char is the start of an escape sequence, or should be executed in ground state...
            {
                if (!_run.empty())
                {
                    // Because the _run above is composed INCLUDING current, we must
                    // trim it off here since we just determined it's actionable
                    // and only pass through everything before it.
                    const auto allLeadingUpTo = _run.substr(0, _run.size() - 1);

                    _engine->ActionPrintString(allLeadingUpTo); // ... print all the chars leading up to it as part of the run...
                    _trace.DispatchPrintRunTrace(allLeadingUpTo);
                }

                _processingIndividually = true; // begin processing future characters individually...
                start = current;
                continue;
            }
            else
            {
                ++current; // Otherwise, add this char to the current run to be printed.
            }
        }
    }

    // When we leave the loop, current has been advanced to the length of the string itself
    // (or one past the array index to the final char) so this `substr` operation doesn't +1
    // to include the final character (unlike the one inside the top of the loop above.)
    _run = start < string.size() ? string.substr(start) : std::wstring_view{};

    // If we're at the end of the string and have remaining un-printed characters,
    if (!_processingIndividually && !_run.empty())
    {
        // print the rest of the characters in the string
        _engine->ActionPrintString(_run);
        _trace.DispatchPrintRunTrace(_run);
    }
    else if (_processingIndividually)
    {
        // One of the "weird things" in VT input is the case of something like
        // <kbd>alt+[</kbd>. In VT, that's encoded as `\x1b[`. However, that's
        // also the start of a CSI, and could be the start of a longer sequence,
        // there's no way to know for sure. For an <kbd>alt+[</kbd> keypress,
        // the parser originally would just sit in the `CsiEntry` state after
        // processing it, which would pollute the following keypress (e.g.
        // <kbd>alt+[</kbd>, <kbd>A</kbd> would be processed like `\x1b[A`,
        // which is _wrong_).
        //
        // Fortunately, for VT input, each keystroke comes in as an individual
        // write operation. So, if at the end of processing a string for the
        // InputEngine, we find that we're not in the Ground state, that implies
        // that we've processed some input, but not dispatched it yet. This
        // block at the end of `ProcessString` will then re-process the
        // undispatched string, but it will ensure that it dispatches on the
        // last character of the string. For our previous `\x1b[` scenario, that
        // means we'll make sure to call `_ActionEscDispatch('[')`., which will
        // properly decode the string as <kbd>alt+[</kbd>.

        if (_engine->FlushAtEndOfString())
        {
            // Reset our state, and put all but the last char in again.
            ResetState();
            // Chars to flush are [pwchSequenceStart, pwchCurr)
            auto wchIter = _run.cbegin();
            while (wchIter < _run.cend() - 1)
            {
                ProcessCharacter(*wchIter);
                wchIter++;
            }
            // Manually execute the last char [pwchCurr]
            switch (_state)
            {
            case VTStates::Ground:
                _ActionExecute(*wchIter);
                break;
            case VTStates::Escape:
            case VTStates::EscapeIntermediate:
                _ActionEscDispatch(*wchIter);
                break;
            case VTStates::CsiEntry:
            case VTStates::CsiIntermediate:
            case VTStates::CsiIgnore:
            case VTStates::CsiParam:
                _ActionCsiDispatch(*wchIter);
                break;
            case VTStates::OscParam:
            case VTStates::OscString:
            case VTStates::OscTermination:
                _ActionOscDispatch(*wchIter);
                break;
            case VTStates::Ss3Entry:
            case VTStates::Ss3Param:
                _ActionSs3Dispatch(*wchIter);
                break;
            }
            // microsoft/terminal#2746: Make sure to return to the ground state
            // after dispatching the characters
            _EnterGround();
        }
        else
        {
            // If the engine doesn't require flushing at the end of the string, we
            // want to cache the partial sequence in case we have to flush the whole
            // thing to the terminal later.
            _cachedSequence = _cachedSequence.value_or(std::wstring{}) + std::wstring{ _run };
        }
    }
}

// Routine Description:
// - Wherever the state machine is, whatever it's going, go back to ground.
//     This is used by conhost to "jiggle the handle" - when VT support is
//     turned off, we don't want any bad state left over for the next input it's turned on for
// Arguments:
// - <none>
// Return Value:
// - <none>
void StateMachine::ResetState() noexcept
{
    _EnterGround();
}

// Routine Description:
// - Takes the given printable character and accumulates it as the new ones digit
//   into the given size_t. All existing value is moved up by 10.
// - For example, if your value had 437 and you put in the printable number 2,
//   this function will update value to 4372.
// - Clamps to 32767 if it gets too big.
// Arguments:
// - wch - Printable character to accumulate into the value (after conversion to number, of course)
// - value - The value to update with the printable character. See example above.
// Return Value:
// - <none> - But really it's the update to the given value parameter.
void StateMachine::_AccumulateTo(const wchar_t wch, size_t& value) noexcept
{
    const size_t digit = wch - L'0';

    value = value * 10 + digit;

    // Values larger than the maximum should be mapped to the largest supported value.
    if (value > MAX_PARAMETER_VALUE)
    {
        value = MAX_PARAMETER_VALUE;
    }
}
