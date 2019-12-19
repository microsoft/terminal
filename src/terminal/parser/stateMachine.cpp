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
    _intermediate{},
    _parameters{},
    _oscString{},
    _processingIndividually(false)
{
    _ActionClear();
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
// - Determines if a character indicates an action that should be taken in the ground state -
//     These are C0 characters and the C1 [single-character] CSI.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsActionableFromGround(const wchar_t wch)
{
    return (wch <= AsciiChars::US) || s_IsC1Csi(wch) || s_IsDelete(wch);
}

// Routine Description:
// - Determines if a character belongs to the C0 escape range.
//   This is character sequences less than a space character (null, backspace, new line, etc.)
//   See also https://en.wikipedia.org/wiki/C0_and_C1_control_codes
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsC0Code(const wchar_t wch)
{
    return (wch >= AsciiChars::NUL && wch <= AsciiChars::ETB) ||
           wch == AsciiChars::EM ||
           (wch >= AsciiChars::FS && wch <= AsciiChars::US);
}

// Routine Description:
// - Determines if a character is a C1 CSI (Control Sequence Introducer)
//   This is a single-character way to start a control sequence, as opposed to "ESC[".
//
//   Not all single-byte codepages support C1 control codes--in some, the range that would
//   be used for C1 codes are instead used for additional graphic characters.
//
//   However, we do not need to worry about confusion whether a single byte \x9b in a
//   single-byte stream represents a C1 CSI or some other glyph, because by the time we
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
bool StateMachine::s_IsC1Csi(const wchar_t wch)
{
    return wch == L'\x9b';
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
bool StateMachine::s_IsIntermediate(const wchar_t wch)
{
    return wch >= L' ' && wch <= L'/'; // 0x20 - 0x2F
}

// Routine Description:
// - Determines if a character is the delete character.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsDelete(const wchar_t wch)
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
bool StateMachine::s_IsEscape(const wchar_t wch)
{
    return wch == AsciiChars::ESC;
}

// Routine Description:
// - Determines if a character is "control sequence" beginning indicator.
//   This immediately follows an escape and signifies a varying length control sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsCsiIndicator(const wchar_t wch)
{
    return wch == L'['; // 0x5B
}

// Routine Description:
// - Determines if a character is a delimiter between two parameters in a "control sequence"
//   This occurs in the middle of a control sequence after escape and CsiIndicator have been recognized
//   between a series of parameters.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsCsiDelimiter(const wchar_t wch)
{
    return wch == L';'; // 0x3B
}

// Routine Description:
// - Determines if a character is a valid parameter value
//   Parameters must be numerical digits.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsCsiParamValue(const wchar_t wch)
{
    return wch >= L'0' && wch <= L'9'; // 0x30 - 0x39
}

// Routine Description:
// - Determines if a character is a private range marker for a control sequence.
//   Private range markers indicate vendor-specific behavior.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsCsiPrivateMarker(const wchar_t wch)
{
    return wch == L'<' || wch == L'=' || wch == L'>' || wch == L'?'; // 0x3C - 0x3F
}

// Routine Description:
// - Determines if a character is invalid in a control sequence
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsCsiInvalid(const wchar_t wch)
{
    return wch == L':'; // 0x3A
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
bool StateMachine::s_IsSs3Indicator(const wchar_t wch)
{
    return wch == L'O'; // 0x4F
}

// Routine Description:
// - Determines if a character is a "Single Shift Select" indicator.
//   This immediately follows an escape and signifies a varying length control string.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsOscIndicator(const wchar_t wch)
{
    return wch == L']'; // 0x5D
}

// Routine Description:
// - Determines if a character is a delimiter between two parameters in a "operating system control sequence"
//   This occurs in the middle of a control sequence after escape and OscIndicator have been recognized,
//   after the paramater indicating which OSC action to take.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsOscDelimiter(const wchar_t wch)
{
    return wch == L';'; // 0x3B
}

// Routine Description:
// - Determines if a character is a valid parameter value for an OSC String,
//     that is, the indicator of which OSC action to take.
//   Parameters must be numerical digits.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsOscParamValue(const wchar_t wch)
{
    return s_IsNumber(wch); // 0x30 - 0x39
}

// Routine Description:
// - Determines if a character should be initiate the end of an OSC sequence.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsOscTerminationInitiator(const wchar_t wch)
{
    return wch == AsciiChars::ESC;
}

// Routine Description:
// - Determines if a character should be ignored in a operating system control sequence
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsOscInvalid(const wchar_t wch)
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
bool StateMachine::s_IsOscTerminator(const wchar_t wch)
{
    return wch == L'\x7' || wch == L'\x9C'; // Bell character or C1 terminator
}

// Routine Description:
// - Determines if a character is a valid number character, 0-9.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool StateMachine::s_IsNumber(const wchar_t wch)
{
    return wch >= L'0' && wch <= L'9'; // 0x30 - 0x39
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionExecute(const wchar_t wch)
{
    _trace.TraceOnExecute(wch);
    _engine->ActionExecute(wch);
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character, with the added
//      information that we're executing it from the Escsape state.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionExecuteFromEscape(const wchar_t wch)
{
    _trace.TraceOnExecuteFromEscape(wch);
    _engine->ActionExecuteFromEscape(wch);
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
    _engine->ActionPrint(wch);
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

    bool fSuccess = _engine->ActionEscDispatch(wch, _intermediate);

    // Trace the result.
    _trace.DispatchSequenceTrace(fSuccess);

    if (!fSuccess)
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

    bool fSuccess = _engine->ActionCsiDispatch(wch, _intermediate, { _parameters.data(), _parameters.size() });

    // Trace the result.
    _trace.DispatchSequenceTrace(fSuccess);

    if (!fSuccess)
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
void StateMachine::_ActionCollect(const wchar_t wch)
{
    _trace.TraceOnAction(L"Collect");

    // store collect data
    if (!_intermediate)
    {
        _intermediate.emplace(wch);
    }
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

    // If we have no parameters and we're about to add one, get the 0 value ready here.
    if (!_parameters.size())
    {
        _parameters.push_back(0);
    }

    // On a delimiter, increase the number of params we've seen.
    // "Empty" params should still count as a param -
    //      eg "\x1b[0;;m" should be three "0" params
    if (wch == L';')
    {
        // Move to next param.
        _parameters.push_back(0);
    }
    else
    {
        // Accumulate the character given into the last (current) parameter
        _AccumulateTo(wch, _parameters.back());
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
    _intermediate.reset();

    _parameters.clear();

    _oscString.clear();
    _oscParameter = 0;

    _engine->ActionClear();
}

// Routine Description:
// - Triggers the Ignore action to indicate that the state machine should eat this character and say nothing.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - <none>
void StateMachine::_ActionIgnore()
{
    // do nothing.
    _trace.TraceOnAction(L"Ignore");
}

// Routine Description:
// - Stores this character as part of the param indicating which OSC action to take.
// Arguments:
// - wch - Character to collect.
// Return Value:
// - <none>
void StateMachine::_ActionOscParam(const wchar_t wch)
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

    bool fSuccess = _engine->ActionOscDispatch(wch, _oscParameter, _oscString);

    // Trace the result.
    _trace.DispatchSequenceTrace(fSuccess);

    if (!fSuccess)
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

    bool fSuccess = _engine->ActionSs3Dispatch(wch, { _parameters.data(), _parameters.size() });

    // Trace the result.
    _trace.DispatchSequenceTrace(fSuccess);

    if (!fSuccess)
    {
        // Suppress it and log telemetry on failed cases
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
void StateMachine::_EnterGround()
{
    _state = VTStates::Ground;
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
void StateMachine::_EnterEscapeIntermediate()
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
void StateMachine::_EnterCsiParam()
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
void StateMachine::_EnterCsiIgnore()
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
void StateMachine::_EnterCsiIntermediate()
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
void StateMachine::_EnterOscParam()
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
void StateMachine::_EnterOscString()
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
void StateMachine::_EnterOscTermination()
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
void StateMachine::_EnterSs3Param()
{
    _state = VTStates::Ss3Param;
    _trace.TraceStateChange(L"Ss3Param");
}

// Routine Description:
// - Processes a character event into an Action that occurs while in the Ground state.
//   Events in this state will:
//   1. Execute C0 control characters
//   2. Handle a C1 Control Sequence Introducer
//   3. Print all other characters
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventGround(const wchar_t wch)
{
    _trace.TraceOnEvent(L"Ground");
    if (s_IsC0Code(wch) || s_IsDelete(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsC1Csi(wch))
    {
        _EnterCsiEntry();
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
    if (s_IsC0Code(wch))
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
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsIntermediate(wch))
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
    else if (s_IsCsiIndicator(wch))
    {
        _EnterCsiEntry();
    }
    else if (s_IsOscIndicator(wch))
    {
        _EnterOscParam();
    }
    else if (s_IsSs3Indicator(wch))
    {
        _EnterSs3Entry();
    }
    else
    {
        _ActionEscDispatch(wch);
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsIntermediate(wch))
    {
        _ActionCollect(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else
    {
        _ActionEscDispatch(wch);
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterCsiIntermediate();
    }
    else if (s_IsCsiInvalid(wch))
    {
        _EnterCsiIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiDelimiter(wch))
    {
        _ActionParam(wch);
        _EnterCsiParam();
    }
    else if (s_IsCsiPrivateMarker(wch))
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsIntermediate(wch))
    {
        _ActionCollect(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiInvalid(wch) || s_IsCsiDelimiter(wch) || s_IsCsiPrivateMarker(wch))
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsIntermediate(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiInvalid(wch) || s_IsCsiDelimiter(wch) || s_IsCsiPrivateMarker(wch))
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiDelimiter(wch))
    {
        _ActionParam(wch);
    }
    else if (s_IsIntermediate(wch))
    {
        _ActionCollect(wch);
        _EnterCsiIntermediate();
    }
    else if (s_IsCsiInvalid(wch) || s_IsCsiPrivateMarker(wch))
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
void StateMachine::_EventOscParam(const wchar_t wch)
{
    _trace.TraceOnEvent(L"OscParam");
    if (s_IsOscTerminator(wch))
    {
        _EnterGround();
    }
    else if (s_IsOscParamValue(wch))
    {
        _ActionOscParam(wch);
    }
    else if (s_IsOscDelimiter(wch))
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
    if (s_IsOscTerminator(wch))
    {
        _ActionOscDispatch(wch);
        _EnterGround();
    }
    else if (s_IsOscTerminationInitiator(wch))
    {
        _EnterOscTermination();
    }
    else if (s_IsOscInvalid(wch))
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
// Arguments:
// - wch - Character that triggered the event
// Return Value:
// - <none>
void StateMachine::_EventOscTermination(const wchar_t wch)
{
    _trace.TraceOnEvent(L"OscTermination");

    _ActionOscDispatch(wch);
    _EnterGround();
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsCsiInvalid(wch))
    {
        // It's safe for us to go into the CSI ignore here, because both SS3 and
        //      CSI sequences ignore characters the same way.
        _EnterCsiIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiDelimiter(wch))
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
    if (s_IsC0Code(wch))
    {
        _ActionExecute(wch);
    }
    else if (s_IsDelete(wch))
    {
        _ActionIgnore();
    }
    else if (s_IsCsiParamValue(wch) || s_IsCsiDelimiter(wch))
    {
        _ActionParam(wch);
    }
    else if (s_IsCsiInvalid(wch) || s_IsCsiPrivateMarker(wch))
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
// - Entry to the state machine. Takes characters one by one and processes them according to the state machine rules.
// Arguments:
// - wch - New character to operate upon
// Return Value:
// - <none>
void StateMachine::ProcessCharacter(const wchar_t wch)
{
    _trace.TraceCharInput(wch);

    // Process "from anywhere" events first.
    if (wch == AsciiChars::CAN ||
        wch == AsciiChars::SUB)
    {
        _ActionExecute(wch);
        _EnterGround();
    }
    else if (s_IsEscape(wch) && _state != VTStates::OscString)
    {
        // Don't go to escape from the OSC string state - ESC can be used to
        //      terminate OSC strings.
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
    // _pwchCurr is incremented after a call to ProcessCharacter to indicate
    //      that pwchCurr was processed.
    // However, if we're here, then the processing of pwchChar triggered the
    //      engine to request the entire sequence get passed through, including pwchCurr.
    return _engine->ActionPassThroughString(_run);
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
        _run = string.substr(start, current - start);

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
            if (s_IsActionableFromGround(string.at(current))) // If the current char is the start of an escape sequence, or should be executed in ground state...
            {
                _engine->ActionPrintString(_run); // ... print all the chars leading up to it as part of the run...
                _trace.DispatchPrintRunTrace(_run);
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

    _run = string.substr(start, current - start);

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
void StateMachine::ResetState()
{
    _EnterGround();
}

// Routine Description:
// - Takes the given printable character and accumulates it as the new ones digit
//   into the given size_t. All existing value is moved up by 10.
// - For example, if your value had 437 and you put in the printable number 2,
//   this function will update value to 4372.
// - Clamps to size_t max if it gets too big.
// Arguments:
// - wch - Printable character to accumulate into the value (after conversion to number, of course)
// - value - The value to update with the printable character. See example above.
// Return Value:
// - <none> - But really it's the update to the given value parameter.
void StateMachine::_AccumulateTo(const wchar_t wch, size_t& value)
{
    const size_t digit = wch - L'0';

    // If we overflow while multiplying and adding, the value is just size_t max.
    if (FAILED(SizeTMult(value, 10, &value)) ||
        FAILED(SizeTAdd(value, digit, &value)))
    {
        value = std::numeric_limits<size_t>().max();
    }
}
