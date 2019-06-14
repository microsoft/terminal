// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"
#include "InputStateMachineEngine.hpp"

#include "../../inc/unicode.hpp"
#include "ascii.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif

using namespace Microsoft::Console::VirtualTerminal;

// The values used by VkKeyScan to encode modifiers in the high order byte
const short KEYSCAN_SHIFT = 1;
const short KEYSCAN_CTRL = 2;
const short KEYSCAN_ALT = 4;

// The values with which VT encodes modifier values.
const short VT_SHIFT = 1;
const short VT_ALT = 2;
const short VT_CTRL = 4;

const size_t WRAPPED_SEQUENCE_MAX_LENGTH = 8;

// For reference, the equivalent INPUT_RECORD values are:
// RIGHT_ALT_PRESSED   0x0001
// LEFT_ALT_PRESSED    0x0002
// RIGHT_CTRL_PRESSED  0x0004
// LEFT_CTRL_PRESSED   0x0008
// SHIFT_PRESSED       0x0010
// NUMLOCK_ON          0x0020
// SCROLLLOCK_ON       0x0040
// CAPSLOCK_ON         0x0080
// ENHANCED_KEY        0x0100

const InputStateMachineEngine::CSI_TO_VKEY InputStateMachineEngine::s_rgCsiMap[]{
    { CsiActionCodes::ArrowUp, VK_UP },
    { CsiActionCodes::ArrowDown, VK_DOWN },
    { CsiActionCodes::ArrowRight, VK_RIGHT },
    { CsiActionCodes::ArrowLeft, VK_LEFT },
    { CsiActionCodes::Home, VK_HOME },
    { CsiActionCodes::End, VK_END },
    { CsiActionCodes::CSI_F1, VK_F1 },
    { CsiActionCodes::CSI_F2, VK_F2 },
    { CsiActionCodes::CSI_F3, VK_F3 },
    { CsiActionCodes::CSI_F4, VK_F4 },
};

const InputStateMachineEngine::GENERIC_TO_VKEY InputStateMachineEngine::s_rgGenericMap[]{
    { GenericKeyIdentifiers::GenericHome, VK_HOME },
    { GenericKeyIdentifiers::Insert, VK_INSERT },
    { GenericKeyIdentifiers::Delete, VK_DELETE },
    { GenericKeyIdentifiers::GenericEnd, VK_END },
    { GenericKeyIdentifiers::Prior, VK_PRIOR },
    { GenericKeyIdentifiers::Next, VK_NEXT },
    { GenericKeyIdentifiers::F5, VK_F5 },
    { GenericKeyIdentifiers::F6, VK_F6 },
    { GenericKeyIdentifiers::F7, VK_F7 },
    { GenericKeyIdentifiers::F8, VK_F8 },
    { GenericKeyIdentifiers::F9, VK_F9 },
    { GenericKeyIdentifiers::F10, VK_F10 },
    { GenericKeyIdentifiers::F11, VK_F11 },
    { GenericKeyIdentifiers::F12, VK_F12 },
};

const InputStateMachineEngine::SS3_TO_VKEY InputStateMachineEngine::s_rgSs3Map[]{
    { Ss3ActionCodes::SS3_F1, VK_F1 },
    { Ss3ActionCodes::SS3_F2, VK_F2 },
    { Ss3ActionCodes::SS3_F3, VK_F3 },
    { Ss3ActionCodes::SS3_F4, VK_F4 },
};

InputStateMachineEngine::InputStateMachineEngine(IInteractDispatch* const pDispatch) :
    InputStateMachineEngine(pDispatch, false)
{
}

InputStateMachineEngine::InputStateMachineEngine(IInteractDispatch* const pDispatch, const bool lookingForDSR) :
    _pDispatch(THROW_IF_NULL_ALLOC(pDispatch)),
    _lookingForDSR(lookingForDSR)
{
}

// Method Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionExecute(const wchar_t wch)
{
    return _DoControlCharacter(wch, false);
}

bool InputStateMachineEngine::_DoControlCharacter(const wchar_t wch, const bool writeAlt)
{
    bool fSuccess = false;
    if (wch == UNICODE_ETX && !writeAlt)
    {
        // This is Ctrl+C, which is handled specially by the host.
        fSuccess = _pDispatch->WriteCtrlC();
    }
    else if (wch >= '\x0' && wch < '\x20')
    {
        // This is a C0 Control Character.
        // This should be translated as Ctrl+(wch+x40)
        wchar_t actualChar = wch;
        bool writeCtrl = true;

        short vkey = 0;
        DWORD dwModifierState = 0;

        switch (wch)
        {
        case L'\b':
            fSuccess = _GenerateKeyFromChar(wch + 0x40, &vkey, nullptr);
            break;
        case L'\r':
            writeCtrl = false;
            fSuccess = _GenerateKeyFromChar(wch, &vkey, nullptr);
            break;
        case L'\x1b':
            // Translate escape as the ESC key, NOT C-[.
            // This means that C-[ won't insert ^[ into the buffer anymore,
            //      which isn't the worst tradeoff.
            vkey = VK_ESCAPE;
            writeCtrl = false;
            fSuccess = true;
            break;
        case L'\t':
            writeCtrl = false;
            fSuccess = _GenerateKeyFromChar(actualChar, &vkey, &dwModifierState);
            break;
        default:
            fSuccess = _GenerateKeyFromChar(actualChar, &vkey, &dwModifierState);
            break;
        }

        if (fSuccess)
        {
            if (writeCtrl)
            {
                WI_SetFlag(dwModifierState, LEFT_CTRL_PRESSED);
            }
            if (writeAlt)
            {
                WI_SetFlag(dwModifierState, LEFT_ALT_PRESSED);
            }

            fSuccess = _WriteSingleKey(actualChar, vkey, dwModifierState);
        }
    }
    else if (wch == '\x7f')
    {
        // Note:
        //  The windows telnet expects to send x7f as DELETE, not backspace.
        //      However, the windows telnetd also wouldn't let you move the
        //      cursor back into the input line, so it wasn't possible to
        //      "delete" any input at all, only backspace.
        //  Because of this, we're treating x7f as backspace, like most
        //      terminals do.
        fSuccess = _WriteSingleKey('\x8', VK_BACK, writeAlt ? LEFT_ALT_PRESSED : 0);
    }
    else
    {
        fSuccess = ActionPrint(wch);
    }
    return fSuccess;
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// This is called from the Escape state in the state machine, indicating the
//      immediately previous character was an 0x1b.
// We need to override this method to properly treat 0x1b + C0 strings as
//      Ctrl+Alt+<char> input sequences.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionExecuteFromEscape(const wchar_t wch)
{
    return _DoControlCharacter(wch, true);
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      character given.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPrint(const wchar_t wch)
{
    short vkey = 0;
    DWORD dwModifierState = 0;
    bool fSuccess = _GenerateKeyFromChar(wch, &vkey, &dwModifierState);
    if (fSuccess)
    {
        fSuccess = _WriteSingleKey(wch, vkey, dwModifierState);
    }
    return fSuccess;
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - rgwch - string to dispatch.
// - cch - length of rgwch
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPrintString(const wchar_t* const rgwch,
                                                const size_t cch)
{
    if (cch == 0)
    {
        return true;
    }
    return _pDispatch->WriteString(rgwch, cch);
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - rgwch - string to dispatch.
// - cch - length of rgwch
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPassThroughString(const wchar_t* const rgwch,
                                                      _In_ size_t const cch)
{
    return ActionPrintString(rgwch, cch);
}

// Method Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - wch - Character to dispatch.
// - cIntermediate - Number of "Intermediate" characters found - such as '!', '?'
// - wchIntermediate - Intermediate character in the sequence, if there was one.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionEscDispatch(const wchar_t wch,
                                                const unsigned short /*cIntermediate*/,
                                                const wchar_t /*wchIntermediate*/)
{
    bool fSuccess = false;

    // 0x7f is DEL, which we treat effectively the same as a ctrl character.
    if (wch == 0x7f)
    {
        fSuccess = _DoControlCharacter(wch, true);
    }
    else
    {
        DWORD dwModifierState = 0;
        short vk = 0;
        fSuccess = _GenerateKeyFromChar(wch, &vk, &dwModifierState);
        if (fSuccess)
        {
            // Alt is definitely pressed in the esc+key case.
            dwModifierState = WI_SetFlag(dwModifierState, LEFT_ALT_PRESSED);

            fSuccess = _WriteSingleKey(wch, vk, dwModifierState);
        }
    }

    return fSuccess;
}

// Method Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - cIntermediate - Number of "Intermediate" characters found - such as '!', '?'
// - wchIntermediate - Intermediate character in the sequence, if there was one.
// - rgusParams - set of numeric parameters collected while pasring the sequence.
// - cParams - number of parameters found.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionCsiDispatch(const wchar_t wch,
                                                const unsigned short /*cIntermediate*/,
                                                const wchar_t /*wchIntermediate*/,
                                                _In_reads_(cParams) const unsigned short* const rgusParams,
                                                const unsigned short cParams)
{
    DWORD dwModifierState = 0;
    short vkey = 0;
    unsigned int uiFunction = 0;
    unsigned int col = 0;
    unsigned int row = 0;

    // This is all the args after the first arg, and the count of args not including the first one.
    const unsigned short* const rgusRemainingArgs = (cParams > 1) ? rgusParams + 1 : rgusParams;
    const unsigned short cRemainingArgs = (cParams >= 1) ? cParams - 1 : 0;

    bool fSuccess = false;
    switch (wch)
    {
    case CsiActionCodes::Generic:
        dwModifierState = _GetGenericKeysModifierState(rgusParams, cParams);
        fSuccess = _GetGenericVkey(rgusParams, cParams, &vkey);
        break;
    // case CsiActionCodes::DSR_DeviceStatusReportResponse:
    case CsiActionCodes::CSI_F3:
        // The F3 case is special - it shares a code with the DeviceStatusResponse.
        // If we're looking for that response, then do that, and break out.
        // Else, fall though to the _GetCursorKeysModifierState handler.
        if (_lookingForDSR)
        {
            fSuccess = true;
            fSuccess = _GetXYPosition(rgusParams, cParams, &row, &col);
            break;
        }
    case CsiActionCodes::ArrowUp:
    case CsiActionCodes::ArrowDown:
    case CsiActionCodes::ArrowRight:
    case CsiActionCodes::ArrowLeft:
    case CsiActionCodes::Home:
    case CsiActionCodes::End:
    case CsiActionCodes::CSI_F1:
    case CsiActionCodes::CSI_F2:
    case CsiActionCodes::CSI_F4:
        dwModifierState = _GetCursorKeysModifierState(rgusParams, cParams);
        fSuccess = _GetCursorKeysVkey(wch, &vkey);
        break;
    case CsiActionCodes::CursorBackTab:
        dwModifierState = SHIFT_PRESSED;
        vkey = VK_TAB;
        fSuccess = true;
        break;
    case CsiActionCodes::DTTERM_WindowManipulation:
        fSuccess = _GetWindowManipulationType(rgusParams,
                                              cParams,
                                              &uiFunction);
        break;
    default:
        fSuccess = false;
        break;
    }

    if (fSuccess)
    {
        switch (wch)
        {
        // case CsiActionCodes::DSR_DeviceStatusReportResponse:
        case CsiActionCodes::CSI_F3:
            // The F3 case is special - it shares a code with the DeviceStatusResponse.
            // If we're looking for that response, then do that, and break out.
            // Else, fall though to the _GetCursorKeysModifierState handler.
            if (_lookingForDSR)
            {
                fSuccess = _pDispatch->MoveCursor(row, col);
                // Right now we're only looking for on initial cursor
                //      position response. After that, only look for F3.
                _lookingForDSR = false;
                break;
            }
            __fallthrough;
        case CsiActionCodes::Generic:
        case CsiActionCodes::ArrowUp:
        case CsiActionCodes::ArrowDown:
        case CsiActionCodes::ArrowRight:
        case CsiActionCodes::ArrowLeft:
        case CsiActionCodes::Home:
        case CsiActionCodes::End:
        case CsiActionCodes::CSI_F1:
        case CsiActionCodes::CSI_F2:
        case CsiActionCodes::CSI_F4:
        case CsiActionCodes::CursorBackTab:
            fSuccess = _WriteSingleKey(vkey, dwModifierState);
            break;
        case CsiActionCodes::DTTERM_WindowManipulation:
            fSuccess = _pDispatch->WindowManipulation(static_cast<DispatchTypes::WindowManipulationType>(uiFunction),
                                                      rgusRemainingArgs,
                                                      cRemainingArgs);
            break;
        default:
            fSuccess = false;
            break;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - rgusParams - set of numeric parameters collected while pasring the sequence.
// - cParams - number of parameters found.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionSs3Dispatch(const wchar_t wch,
                                                _In_reads_(_Param_(3)) const unsigned short* const /*rgusParams*/,
                                                const unsigned short /*cParams*/)
{
    // Ss3 sequence keys aren't modified.
    // When F1-F4 *are* modified, they're sent as CSI sequences, not SS3's.
    DWORD dwModifierState = 0;
    short vkey = 0;

    bool fSuccess = _GetSs3KeysVkey(wch, &vkey);

    if (fSuccess)
    {
        fSuccess = _WriteSingleKey(vkey, dwModifierState);
    }

    return fSuccess;
}

// Method Description:
// - Triggers the Clear action to indicate that the state machine should erase
//      all internal state.
// Arguments:
// - <none>
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionClear()
{
    return true;
}

// Method Description:
// - Triggers the Ignore action to indicate that the state machine should eat
//      this character and say nothing.
// Arguments:
// - <none>
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionIgnore()
{
    return true;
}

// Method Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch. This will be a BEL or ST char.
// - sOscParam - identifier of the OSC action to perform
// - pwchOscStringBuffer - OSC string we've collected. NOT null terminated.
// - cchOscString - length of pwchOscStringBuffer
// Return Value:
// - true if we handled the dsipatch.
bool InputStateMachineEngine::ActionOscDispatch(const wchar_t /*wch*/,
                                                const unsigned short /*sOscParam*/,
                                                _Inout_updates_(_Param_(4)) wchar_t* const /*pwchOscStringBuffer*/,
                                                const unsigned short /*cchOscString*/)
{
    return false;
}

// Method Description:
// - Writes a sequence of keypresses to the buffer based on the wch,
//      vkey and modifiers passed in. Will create both the appropriate key downs
//      and ups for that key for writing to the input. Will also generate
//      keypresses for pressing the modifier keys while typing that character.
//  If rgInput isn't big enough, then it will stop writing when it's filled.
// Arguments:
// - wch - the character to write to the input callback.
// - vkey - the VKEY of the key to write to the input callback.
// - dwModifierState - the modifier state to write with the key.
// - rgInput - the buffer of characters to write the keypresses to. Can write
//      up to 8 records to this buffer.
// - cInput - the size of rgInput. This should be at least WRAPPED_SEQUENCE_MAX_LENGTH
// Return Value:
// - the number of records written, or 0 if the buffer wasn't big enough.
size_t InputStateMachineEngine::_GenerateWrappedSequence(const wchar_t wch,
                                                         const short vkey,
                                                         const DWORD dwModifierState,
                                                         _Inout_updates_(cInput) INPUT_RECORD* rgInput,
                                                         const size_t cInput)
{
    // TODO: Reuse the clipboard functions for generating input for characters
    //       that aren't on the current keyboard.
    // MSFT:13994942
    if (cInput < WRAPPED_SEQUENCE_MAX_LENGTH)
    {
        return 0;
    }

    const bool fShift = WI_IsFlagSet(dwModifierState, SHIFT_PRESSED);
    const bool fCtrl = WI_IsFlagSet(dwModifierState, LEFT_CTRL_PRESSED);
    const bool fAlt = WI_IsFlagSet(dwModifierState, LEFT_ALT_PRESSED);

    size_t index = 0;
    INPUT_RECORD* next = &rgInput[0];

    DWORD dwCurrentModifiers = 0;

    if (fShift)
    {
        WI_SetFlag(dwCurrentModifiers, SHIFT_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = TRUE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }
    if (fAlt)
    {
        WI_SetFlag(dwCurrentModifiers, LEFT_ALT_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = TRUE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }
    if (fCtrl)
    {
        WI_SetFlag(dwCurrentModifiers, LEFT_CTRL_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = TRUE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }

    size_t added = _GetSingleKeypress(wch, vkey, dwCurrentModifiers, next, cInput - index);

    // if _GetSingleKeypress added more than two events we might overflow the buffer
    if (added > 2)
    {
        return index;
    }

    next += added;
    index += added;

    if (fCtrl)
    {
        WI_ClearFlag(dwCurrentModifiers, LEFT_CTRL_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = FALSE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }
    if (fAlt)
    {
        WI_ClearFlag(dwCurrentModifiers, LEFT_ALT_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = FALSE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }
    if (fShift)
    {
        WI_ClearFlag(dwCurrentModifiers, SHIFT_PRESSED);
        next->EventType = KEY_EVENT;
        next->Event.KeyEvent.bKeyDown = FALSE;
        next->Event.KeyEvent.dwControlKeyState = dwCurrentModifiers;
        next->Event.KeyEvent.wRepeatCount = 1;
        next->Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC));
        next->Event.KeyEvent.uChar.UnicodeChar = 0x0;
        next++;
        index++;
    }

    return index;
}

// Method Description:
// - Writes a single character keypress to the input buffer. This writes both
//      the keydown and keyup events.
// Arguments:
// - wch - the character to write to the buffer.
// - vkey - the VKEY of the key to write to the buffer.
// - dwModifierState - the modifier state to write with the key.
// - rgInput - the buffer of characters to write the keypress to. Will always
//      write to the first two positions in the buffer.
// - cRecords - the size of rgInput
// Return Value:
// - the number of input records written.
size_t InputStateMachineEngine::_GetSingleKeypress(const wchar_t wch,
                                                   const short vkey,
                                                   const DWORD dwModifierState,
                                                   _Inout_updates_(cRecords) INPUT_RECORD* const rgInput,
                                                   const size_t cRecords)
{
    FAIL_FAST_IF(!(cRecords >= 2));
    if (cRecords < 2)
    {
        return 0;
    }

    rgInput[0].EventType = KEY_EVENT;
    rgInput[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInput[0].Event.KeyEvent.dwControlKeyState = dwModifierState;
    rgInput[0].Event.KeyEvent.wRepeatCount = 1;
    rgInput[0].Event.KeyEvent.wVirtualKeyCode = vkey;
    rgInput[0].Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    rgInput[0].Event.KeyEvent.uChar.UnicodeChar = wch;

    rgInput[1] = rgInput[0];
    rgInput[1].Event.KeyEvent.bKeyDown = FALSE;

    return 2;
}

// Method Description:
// - Writes a sequence of keypresses to the input callback based on the wch,
//      vkey and modifiers passed in. Will create both the appropriate key downs
//      and ups for that key for writing to the input. Will also generate
//      keypresses for pressing the modifier keys while typing that character.
// Arguments:
// - wch - the character to write to the input callback.
// - vkey - the VKEY of the key to write to the input callback.
// - dwModifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
bool InputStateMachineEngine::_WriteSingleKey(const wchar_t wch, const short vkey, const DWORD dwModifierState)
{
    // At most 8 records - 2 for each of shift,ctrl,alt up and down, and 2 for the actual key up and down.
    INPUT_RECORD rgInput[WRAPPED_SEQUENCE_MAX_LENGTH];
    size_t cInput = _GenerateWrappedSequence(wch, vkey, dwModifierState, rgInput, WRAPPED_SEQUENCE_MAX_LENGTH);

    std::deque<std::unique_ptr<IInputEvent>> inputEvents = IInputEvent::Create(gsl::make_span(rgInput, cInput));

    return _pDispatch->WriteInput(inputEvents);
}

// Method Description:
// - Helper for writing a single key to the input when you only know the vkey.
//      Will automatically get the wchar_t associated with that vkey.
// Arguments:
// - vkey - the VKEY of the key to write to the input callback.
// - dwModifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
bool InputStateMachineEngine::_WriteSingleKey(const short vkey, const DWORD dwModifierState)
{
    wchar_t wch = (wchar_t)MapVirtualKey(vkey, MAPVK_VK_TO_CHAR);
    return _WriteSingleKey(wch, vkey, dwModifierState);
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a cursor keys
//      sequence. This is for Arrow keys, Home, End, etc.
// Arguments:
// - rgusParams - the set of parameters to get the modifier state from.
// - cParams - the number of elements in rgusParams
// Return Value:
// - the INPUT_RECORD comaptible modifier state.
DWORD InputStateMachineEngine::_GetCursorKeysModifierState(_In_reads_(cParams) const unsigned short* const rgusParams, const unsigned short cParams)
{
    // Both Cursor keys and generic keys keep their modifiers in the same index.
    return _GetGenericKeysModifierState(rgusParams, cParams);
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a "Generic"
//      keypress - one who's sequence is terminated with a '~'.
// Arguments:
// - rgusParams - the set of parameters to get the modifier state from.
// - cParams - the number of elements in rgusParams
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetGenericKeysModifierState(_In_reads_(cParams) const unsigned short* const rgusParams, const unsigned short cParams)
{
    DWORD dwModifiers = 0;
    if (_IsModified(cParams) && cParams >= 2)
    {
        dwModifiers = _GetModifier(rgusParams[1]);
    }
    return dwModifiers;
}

// Method Description:
// - Determines if a set of parameters indicates a modified keypress
// Arguments:
// - cParams - the nummber of parameters we've collected in this sequence
// Return Value:
// - true iff the sequence is a modified sequence.
bool InputStateMachineEngine::_IsModified(const unsigned short cParams)
{
    // modified input either looks like
    // \x1b[1;mA or \x1b[17;m~
    // Both have two parameters
    return cParams == 2;
}

// Method Description:
// - Converts a VT encoded modifier param into a INPUT_RECORD compatible one.
// Arguments:
// - modifierParam - the VT modifier value to convert
// Return Value:
// - The equivalent INPUT_RECORD modifier value.
DWORD InputStateMachineEngine::_GetModifier(const unsigned short modifierParam)
{
    // VT Modifiers are 1+(modifier flags)
    unsigned short vtParam = modifierParam - 1;
    DWORD modifierState = modifierParam > 0 ? ENHANCED_KEY : 0;

    bool fShift = WI_IsFlagSet(vtParam, VT_SHIFT);
    bool fAlt = WI_IsFlagSet(vtParam, VT_ALT);
    bool fCtrl = WI_IsFlagSet(vtParam, VT_CTRL);
    return modifierState | (fShift ? SHIFT_PRESSED : 0) | (fAlt ? LEFT_ALT_PRESSED : 0) | (fCtrl ? LEFT_CTRL_PRESSED : 0);
}

// Method Description:
// - Gets the Vkey form the generic keys table associated with a particular
//   identifier code. The identifier code will be the first param in rgusParams.
// Arguments:
// - rgusParams: an array of shorts where the first is the identifier of the key
//      we're looking for.
// - cParams: number of params in rgusParams
// - pVkey: Recieves the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetGenericVkey(_In_reads_(cParams) const unsigned short* const rgusParams, const unsigned short cParams, _Out_ short* const pVkey) const
{
    *pVkey = 0;
    if (cParams < 1)
    {
        return false;
    }

    const unsigned short identifier = rgusParams[0];
    for (int i = 0; i < ARRAYSIZE(s_rgGenericMap); i++)
    {
        GENERIC_TO_VKEY mapping = s_rgGenericMap[i];
        if (mapping.Identifier == identifier)
        {
            *pVkey = mapping.vkey;
            return true;
        }
    }
    return false;
}

// Method Description:
// - Gets the Vkey from the CSI codes table associated with a particular character.
// Arguments:
// - wch: the wchar_t to get the mapped vkey of.
// - pVkey: Recieves the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetCursorKeysVkey(const wchar_t wch, _Out_ short* const pVkey) const
{
    *pVkey = 0;
    for (int i = 0; i < ARRAYSIZE(s_rgCsiMap); i++)
    {
        CSI_TO_VKEY mapping = s_rgCsiMap[i];
        if (mapping.Action == wch)
        {
            *pVkey = mapping.vkey;
            return true;
        }
    }

    return false;
}

// Method Description:
// - Gets the Vkey from the SS3 codes table associated with a particular character.
// Arguments:
// - wch: the wchar_t to get the mapped vkey of.
// - pVkey: Recieves the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetSs3KeysVkey(const wchar_t wch, _Out_ short* const pVkey) const
{
    *pVkey = 0;
    for (int i = 0; i < ARRAYSIZE(s_rgSs3Map); i++)
    {
        SS3_TO_VKEY mapping = s_rgSs3Map[i];
        if (mapping.Action == wch)
        {
            *pVkey = mapping.vkey;
            return true;
        }
    }

    return false;
}

// Method Description:
// - Gets the Vkey and modifier state that's associated with a particular char.
// Arguments:
// - wch: the wchar_t to get the vkey and modifier state of.
// - pVkey: Recieves the vkey
// - pdwModifierState: Recieves the modifier state
// Return Value:
// <none>
bool InputStateMachineEngine::_GenerateKeyFromChar(const wchar_t wch,
                                                   _Out_ short* const pVkey,
                                                   _Out_ DWORD* const pdwModifierState)
{
    // Low order byte is key, high order is modifiers
    short keyscan = VkKeyScanW(wch);

    short vkey = LOBYTE(keyscan);

    short keyscanModifiers = HIBYTE(keyscan);

    if (vkey == -1 && keyscanModifiers == -1)
    {
        return false;
    }

    // Because of course, these are not the same flags.
    short dwModifierState = 0 |
                            (WI_IsFlagSet(keyscanModifiers, KEYSCAN_SHIFT) ? SHIFT_PRESSED : 0) |
                            (WI_IsFlagSet(keyscanModifiers, KEYSCAN_CTRL) ? LEFT_CTRL_PRESSED : 0) |
                            (WI_IsFlagSet(keyscanModifiers, KEYSCAN_ALT) ? LEFT_ALT_PRESSED : 0);

    if (pVkey != nullptr)
    {
        *pVkey = vkey;
    }
    if (pdwModifierState != nullptr)
    {
        *pdwModifierState = dwModifierState;
    }
    return true;
}

// Method Description:
// - Returns true if the engine should dispatch on the last charater of a string
//      always, even if the sequence hasn't normally dispatched.
//   If this is false, the engine will persist its state across calls to
//      ProcessString, and dispatch only at the end of the sequence.
// Return Value:
// - True iff we should manually dispatch on the last character of a string.
bool InputStateMachineEngine::FlushAtEndOfString() const
{
    return true;
}

// Routine Description:
// - Returns true if the engine should dispatch control characters in the Escape
//      state. Typically, control characters are immediately executed in the
//      Escape state without returning to ground. If this returns true, the
//      state machine will instead call ActionExecuteFromEscape and then enter
//      the Ground state when a control character is encountered in the escape
//      state.
// Return Value:
// - True iff we should return to the Ground state when the state machine
//      encounters a Control (C0) character in the Escape state.
bool InputStateMachineEngine::DispatchControlCharsFromEscape() const
{
    return true;
}

// Routine Description:
// - Returns false if the engine wants to be able to collect intermediate
//   characters in the Escape state. We do _not_ want to buffer any characters
//   as intermediates, because we use ESC as a prefix to indicate a key was
//   pressed while Alt was pressed.
// Return Value:
// - True iff we should dispatch in the Escape state when we encounter a
//   Intermediate character.
bool InputStateMachineEngine::DispatchIntermediatesFromEscape() const
{
    return true;
}

// Method Description:
// - Retrieves the type of window manipulation operation from the parameter pool
//      stored during Param actions.
//  This is kept seperate from the output version, as there may be
//      codes that are supported in one direction but not the other.
// Arguments:
// - rgusParams - Array of parameters collected
// - cParams - Number of parameters we've collected
// - puiFunction - Memory location to receive the function type
// Return Value:
// - True iff we successfully pulled the function type from the parameters
bool InputStateMachineEngine::_GetWindowManipulationType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                         const unsigned short cParams,
                                                         _Out_ unsigned int* const puiFunction) const
{
    bool fSuccess = false;
    *puiFunction = DispatchTypes::WindowManipulationType::Invalid;

    if (cParams > 0)
    {
        switch (rgusParams[0])
        {
        case DispatchTypes::WindowManipulationType::RefreshWindow:
            *puiFunction = DispatchTypes::WindowManipulationType::RefreshWindow;
            fSuccess = true;
            break;
        case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
            *puiFunction = DispatchTypes::WindowManipulationType::ResizeWindowInCharacters;
            fSuccess = true;
            break;
        default:
            fSuccess = false;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves an X/Y coordinate pair for a cursor operation from the parameter pool stored during Param actions.
// Arguments:
// - puiLine - Memory location to receive the Y/Line/Row position
// - puiColumn - Memory location to receive the X/Column position
// Return Value:
// - True if we successfully pulled the cursor coordinates from the parameters we've stored. False otherwise.
_Success_(return ) bool InputStateMachineEngine::_GetXYPosition(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                const unsigned short cParams,
                                                                _Out_ unsigned int* const puiLine,
                                                                _Out_ unsigned int* const puiColumn) const
{
    bool fSuccess = true;
    *puiLine = s_uiDefaultLine;
    *puiColumn = s_uiDefaultColumn;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
    }
    else if (cParams == 1)
    {
        // If there's only one param, leave the default for the column, and retrieve the specified row.
        *puiLine = rgusParams[0];
    }
    else if (cParams == 2)
    {
        // If there are exactly two parameters, use them.
        *puiLine = rgusParams[0];
        *puiColumn = rgusParams[1];
    }
    else
    {
        fSuccess = false;
    }

    // Distances of 0 should be changed to 1.
    if (*puiLine == 0)
    {
        *puiLine = s_uiDefaultLine;
    }

    if (*puiColumn == 0)
    {
        *puiColumn = s_uiDefaultColumn;
    }

    return fSuccess;
}
