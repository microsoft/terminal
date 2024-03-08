// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"
#include "InputStateMachineEngine.hpp"

#include "../../inc/unicode.hpp"
#include "ascii.hpp"
#include "../../interactivity/inc/VtApiRedirection.hpp"

using namespace Microsoft::Console::VirtualTerminal;

struct CsiToVkey
{
    CsiActionCodes action;
    short vkey;
};

static constexpr std::array<CsiToVkey, 10> s_csiMap = {
    CsiToVkey{ CsiActionCodes::ArrowUp, VK_UP },
    CsiToVkey{ CsiActionCodes::ArrowDown, VK_DOWN },
    CsiToVkey{ CsiActionCodes::ArrowRight, VK_RIGHT },
    CsiToVkey{ CsiActionCodes::ArrowLeft, VK_LEFT },
    CsiToVkey{ CsiActionCodes::Home, VK_HOME },
    CsiToVkey{ CsiActionCodes::End, VK_END },
    CsiToVkey{ CsiActionCodes::CSI_F1, VK_F1 },
    CsiToVkey{ CsiActionCodes::CSI_F2, VK_F2 },
    CsiToVkey{ CsiActionCodes::CSI_F3, VK_F3 },
    CsiToVkey{ CsiActionCodes::CSI_F4, VK_F4 }
};

static bool operator==(const CsiToVkey& pair, const VTID id) noexcept
{
    return pair.action == id;
}

struct GenericToVkey
{
    GenericKeyIdentifiers identifier;
    short vkey;
};

static constexpr std::array<GenericToVkey, 14> s_genericMap = {
    GenericToVkey{ GenericKeyIdentifiers::GenericHome, VK_HOME },
    GenericToVkey{ GenericKeyIdentifiers::Insert, VK_INSERT },
    GenericToVkey{ GenericKeyIdentifiers::Delete, VK_DELETE },
    GenericToVkey{ GenericKeyIdentifiers::GenericEnd, VK_END },
    GenericToVkey{ GenericKeyIdentifiers::Prior, VK_PRIOR },
    GenericToVkey{ GenericKeyIdentifiers::Next, VK_NEXT },
    GenericToVkey{ GenericKeyIdentifiers::F5, VK_F5 },
    GenericToVkey{ GenericKeyIdentifiers::F6, VK_F6 },
    GenericToVkey{ GenericKeyIdentifiers::F7, VK_F7 },
    GenericToVkey{ GenericKeyIdentifiers::F8, VK_F8 },
    GenericToVkey{ GenericKeyIdentifiers::F9, VK_F9 },
    GenericToVkey{ GenericKeyIdentifiers::F10, VK_F10 },
    GenericToVkey{ GenericKeyIdentifiers::F11, VK_F11 },
    GenericToVkey{ GenericKeyIdentifiers::F12, VK_F12 },
};

static bool operator==(const GenericToVkey& pair, const GenericKeyIdentifiers identifier) noexcept
{
    return pair.identifier == identifier;
}

struct Ss3ToVkey
{
    Ss3ActionCodes action;
    short vkey;
};

static constexpr std::array<Ss3ToVkey, 10> s_ss3Map = {
    Ss3ToVkey{ Ss3ActionCodes::ArrowUp, VK_UP },
    Ss3ToVkey{ Ss3ActionCodes::ArrowDown, VK_DOWN },
    Ss3ToVkey{ Ss3ActionCodes::ArrowRight, VK_RIGHT },
    Ss3ToVkey{ Ss3ActionCodes::ArrowLeft, VK_LEFT },
    Ss3ToVkey{ Ss3ActionCodes::End, VK_END },
    Ss3ToVkey{ Ss3ActionCodes::Home, VK_HOME },
    Ss3ToVkey{ Ss3ActionCodes::SS3_F1, VK_F1 },
    Ss3ToVkey{ Ss3ActionCodes::SS3_F2, VK_F2 },
    Ss3ToVkey{ Ss3ActionCodes::SS3_F3, VK_F3 },
    Ss3ToVkey{ Ss3ActionCodes::SS3_F4, VK_F4 },
};

static bool operator==(const Ss3ToVkey& pair, const Ss3ActionCodes code) noexcept
{
    return pair.action == code;
}

InputStateMachineEngine::InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch) :
    InputStateMachineEngine(std::move(pDispatch), false)
{
}

InputStateMachineEngine::InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch, const bool lookingForDSR) :
    _pDispatch(std::move(pDispatch)),
    _lookingForDSR(lookingForDSR),
    _pfnFlushToInputQueue(nullptr),
    _doubleClickTime(std::chrono::milliseconds(GetDoubleClickTime()))
{
    THROW_HR_IF_NULL(E_INVALIDARG, _pDispatch.get());
}

bool InputStateMachineEngine::EncounteredWin32InputModeSequence() const noexcept
{
    return _encounteredWin32InputModeSequence;
}

void InputStateMachineEngine::SetLookingForDSR(const bool looking) noexcept
{
    _lookingForDSR = looking;
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

// Routine Description:
// - Writes a control character into the buffer. Think characters like tab, backspace, etc.
// Arguments:
// - wch - The character to write
// - writeAlt - Pass in the alt-state information here as it's not embedded
// Return Value:
// - True if successfully generated and written. False otherwise.
bool InputStateMachineEngine::_DoControlCharacter(const wchar_t wch, const bool writeAlt)
{
    auto success = false;
    if (wch == UNICODE_ETX && !writeAlt)
    {
        // This is Ctrl+C, which is handled specially by the host.
        static constexpr auto keyDown = SynthesizeKeyEvent(true, 1, L'C', 0, UNICODE_ETX, LEFT_CTRL_PRESSED);
        static constexpr auto keyUp = SynthesizeKeyEvent(false, 1, L'C', 0, UNICODE_ETX, LEFT_CTRL_PRESSED);
        _pDispatch->WriteCtrlKey(keyDown);
        _pDispatch->WriteCtrlKey(keyUp);
        success = true;
    }
    else if (wch >= '\x0' && wch < '\x20')
    {
        // This is a C0 Control Character.
        // This should be translated as Ctrl+(wch+x40)
        auto actualChar = wch;
        auto writeCtrl = true;

        short vkey = 0;
        DWORD modifierState = 0;

        switch (wch)
        {
        case L'\b':
            // Process Ctrl+Bksp to delete whole words
            actualChar = '\x7f';
            success = _GenerateKeyFromChar(actualChar, vkey, modifierState);
            modifierState = 0;
            break;
        case L'\r':
            writeCtrl = false;
            success = _GenerateKeyFromChar(wch, vkey, modifierState);
            modifierState = 0;
            break;
        case L'\x1b':
            // Translate escape as the ESC key, NOT C-[.
            // This means that C-[ won't insert ^[ into the buffer anymore,
            //      which isn't the worst tradeoff.
            vkey = VK_ESCAPE;
            writeCtrl = false;
            success = true;
            break;
        case L'\t':
            writeCtrl = false;
            success = _GenerateKeyFromChar(actualChar, vkey, modifierState);
            break;
        default:
            success = _GenerateKeyFromChar(actualChar, vkey, modifierState);
            break;
        }

        if (success)
        {
            if (writeCtrl)
            {
                WI_SetFlag(modifierState, LEFT_CTRL_PRESSED);
            }
            if (writeAlt)
            {
                WI_SetFlag(modifierState, LEFT_ALT_PRESSED);
            }

            success = _WriteSingleKey(actualChar, vkey, modifierState);
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
        success = _WriteSingleKey('\x8', VK_BACK, writeAlt ? LEFT_ALT_PRESSED : 0);
    }
    else
    {
        success = ActionPrint(wch);
    }
    return success;
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
    if (_pDispatch->IsVtInputEnabled() && _pfnFlushToInputQueue)
    {
        return _pfnFlushToInputQueue();
    }

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
    DWORD modifierState = 0;
    auto success = _GenerateKeyFromChar(wch, vkey, modifierState);
    if (success)
    {
        success = _WriteSingleKey(wch, vkey, modifierState);
    }
    return success;
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPrintString(const std::wstring_view string)
{
    if (string.empty())
    {
        return true;
    }
    return _pDispatch->WriteString(string);
}

// Method Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionPassThroughString(const std::wstring_view string)
{
    if (_pDispatch->IsVtInputEnabled())
    {
        // Synthesize string into key events that we'll write to the buffer
        // similar to TerminalInput::_SendInputSequence
        if (!string.empty())
        {
            InputEventQueue inputEvents;
            for (const auto& wch : string)
            {
                inputEvents.push_back(SynthesizeKeyEvent(true, 1, 0, 0, wch, 0));
            }
            return _pDispatch->WriteInput(inputEvents);
        }
    }
    return ActionPrintString(string);
}

// Method Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionEscDispatch(const VTID id)
{
    if (_pDispatch->IsVtInputEnabled() && _pfnFlushToInputQueue)
    {
        return _pfnFlushToInputQueue();
    }

    auto success = false;

    // There are no intermediates, so the id is effectively the final char.
    const auto wch = gsl::narrow_cast<wchar_t>(id);

    // 0x7f is DEL, which we treat effectively the same as a ctrl character.
    if (wch == 0x7f)
    {
        success = _DoControlCharacter(wch, true);
    }
    else
    {
        DWORD modifierState = 0;
        short vk = 0;
        success = _GenerateKeyFromChar(wch, vk, modifierState);
        if (success)
        {
            // Alt is definitely pressed in the esc+key case.
            modifierState = WI_SetFlag(modifierState, LEFT_ALT_PRESSED);

            success = _WriteSingleKey(wch, vk, modifierState);
        }
    }

    return success;
}

// Method Description:
// - Triggers the Vt52EscDispatch action to indicate that the listener should handle
//      a VT52 escape sequence. These sequences start with ESC and a single letter,
//      sometimes followed by parameters.
// Arguments:
// - id - Identifier of the VT52 sequence to dispatch.
// - parameters - Set of parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionVt52EscDispatch(const VTID /*id*/, const VTParameters /*parameters*/) noexcept
{
    // VT52 escape sequences are not used in the input state machine.
    return false;
}

// Method Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)
{
    // GH#4999 - If the client was in VT input mode, but we received a
    // win32-input-mode sequence, then _don't_ passthrough the sequence to the
    // client. It's impossibly unlikely that the client actually wanted
    // win32-input-mode, and if they did, then we'll just translate the
    // INPUT_RECORD back to the same sequence we say here later on, when the
    // client reads it.
    //
    // Focus events in conpty are special, so don't flush those through either.
    // See GH#12799, GH#12900 for details
    if (_pDispatch->IsVtInputEnabled() &&
        _pfnFlushToInputQueue &&
        id != CsiActionCodes::Win32KeyboardInput &&
        id != CsiActionCodes::FocusIn &&
        id != CsiActionCodes::FocusOut)
    {
        return _pfnFlushToInputQueue();
    }

    DWORD modifierState = 0;
    short vkey = 0;

    auto success = false;
    switch (id)
    {
    case CsiActionCodes::MouseDown:
    case CsiActionCodes::MouseUp:
    {
        DWORD buttonState = 0;
        DWORD eventFlags = 0;
        const auto firstParameter = parameters.at(0).value_or(0);
        const til::point uiPos{ parameters.at(1) - 1, parameters.at(2) - 1 };

        modifierState = _GetSGRMouseModifierState(firstParameter);
        success = _UpdateSGRMouseButtonState(id, firstParameter, buttonState, eventFlags, uiPos);
        success = success && _WriteMouseEvent(uiPos, buttonState, modifierState, eventFlags);
        break;
    }
    // case CsiActionCodes::DSR_DeviceStatusReportResponse:
    case CsiActionCodes::CSI_F3:
        // The F3 case is special - it shares a code with the DeviceStatusResponse.
        // If we're looking for that response, then do that, and break out.
        // Else, fall though to the _GetCursorKeysModifierState handler.
        if (_lookingForDSR)
        {
            success = _pDispatch->MoveCursor(parameters.at(0), parameters.at(1));
            // Right now we're only looking for on initial cursor
            //      position response. After that, only look for F3.
            _lookingForDSR = false;
            break;
        }
        [[fallthrough]];
    case CsiActionCodes::ArrowUp:
    case CsiActionCodes::ArrowDown:
    case CsiActionCodes::ArrowRight:
    case CsiActionCodes::ArrowLeft:
    case CsiActionCodes::Home:
    case CsiActionCodes::End:
    case CsiActionCodes::CSI_F1:
    case CsiActionCodes::CSI_F2:
    case CsiActionCodes::CSI_F4:
        success = _GetCursorKeysVkey(id, vkey);
        modifierState = _GetCursorKeysModifierState(parameters, id);
        success = success && _WriteSingleKey(vkey, modifierState);
        break;
    case CsiActionCodes::Generic:
        success = _GetGenericVkey(parameters.at(0), vkey);
        modifierState = _GetGenericKeysModifierState(parameters);
        success = success && _WriteSingleKey(vkey, modifierState);
        break;
    case CsiActionCodes::CursorBackTab:
        success = _WriteSingleKey(VK_TAB, SHIFT_PRESSED);
        break;
    case CsiActionCodes::DTTERM_WindowManipulation:
        success = _pDispatch->WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
        break;
    case CsiActionCodes::FocusIn:
        success = _pDispatch->FocusChanged(true);
        break;
    case CsiActionCodes::FocusOut:
        success = _pDispatch->FocusChanged(false);
        break;
    case CsiActionCodes::Win32KeyboardInput:
    {
        // Use WriteCtrlKey here, even for keys that _aren't_ control keys,
        // because that will take extra steps to make sure things like
        // Ctrl+C, Ctrl+Break are handled correctly.
        const auto key = _GenerateWin32Key(parameters);
        success = _pDispatch->WriteCtrlKey(key);
        _encounteredWin32InputModeSequence = true;
        break;
    }
    default:
        success = false;
        break;
    }

    return success;
}

// Routine Description:
// - Triggers the DcsDispatch action to indicate that the listener should handle
//      a control sequence. Returns the handler function that is to be used to
//      process the subsequent data string characters in the sequence.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - the data string handler function or nullptr if the sequence is not supported
IStateMachineEngine::StringHandler InputStateMachineEngine::ActionDcsDispatch(const VTID /*id*/, const VTParameters /*parameters*/) noexcept
{
    // DCS escape sequences are not used in the input state machine.
    return nullptr;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionSs3Dispatch(const wchar_t wch, const VTParameters /*parameters*/)
{
    if (_pDispatch->IsVtInputEnabled() && _pfnFlushToInputQueue)
    {
        return _pfnFlushToInputQueue();
    }

    // Ss3 sequence keys aren't modified.
    // When F1-F4 *are* modified, they're sent as CSI sequences, not SS3's.
    const DWORD modifierState = 0;
    short vkey = 0;

    auto success = _GetSs3KeysVkey(wch, vkey);

    if (success)
    {
        success = _WriteSingleKey(vkey, modifierState);
    }

    return success;
}

// Method Description:
// - Triggers the Clear action to indicate that the state machine should erase
//      all internal state.
// Arguments:
// - <none>
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionClear() noexcept
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
bool InputStateMachineEngine::ActionIgnore() noexcept
{
    return true;
}

// Method Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dispatch.
bool InputStateMachineEngine::ActionOscDispatch(const size_t /*parameter*/, const std::wstring_view /*string*/) noexcept
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
// - modifierState - the modifier state to write with the key.
// - input - the buffer of characters to write the keypresses to. Can write
//      up to 8 records to this buffer.
// Return Value:
// - the number of records written, or 0 if the buffer wasn't big enough.
void InputStateMachineEngine::_GenerateWrappedSequence(const wchar_t wch,
                                                       const short vkey,
                                                       const DWORD modifierState,
                                                       InputEventQueue& input)
{
    input.reserve(input.size() + 8);

    // TODO: Reuse the clipboard functions for generating input for characters
    //       that aren't on the current keyboard.
    // MSFT:13994942

    const auto shift = WI_IsFlagSet(modifierState, SHIFT_PRESSED);
    const auto ctrl = WI_IsFlagSet(modifierState, LEFT_CTRL_PRESSED);
    const auto alt = WI_IsFlagSet(modifierState, LEFT_ALT_PRESSED);

    auto next = SynthesizeKeyEvent(true, 1, 0, 0, 0, 0);
    DWORD currentModifiers = 0;

    if (shift)
    {
        WI_SetFlag(currentModifiers, SHIFT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (alt)
    {
        WI_SetFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (ctrl)
    {
        WI_SetFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }

    // Use modifierState instead of currentModifiers here.
    // This allows other modifiers like ENHANCED_KEY to get
    //    through on the KeyPress.
    _GetSingleKeypress(wch, vkey, modifierState, input);

    next.Event.KeyEvent.bKeyDown = FALSE;

    if (ctrl)
    {
        WI_ClearFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (alt)
    {
        WI_ClearFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
    if (shift)
    {
        WI_ClearFlag(currentModifiers, SHIFT_PRESSED);
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC));
        input.push_back(next);
    }
}

// Method Description:
// - Writes a single character keypress to the input buffer. This writes both
//      the keydown and keyup events.
// Arguments:
// - wch - the character to write to the buffer.
// - vkey - the VKEY of the key to write to the buffer.
// - modifierState - the modifier state to write with the key.
// - input - the buffer of characters to write the keypress to. Will always
//      write to the first two positions in the buffer.
// Return Value:
// - the number of input records written.
void InputStateMachineEngine::_GetSingleKeypress(const wchar_t wch,
                                                 const short vkey,
                                                 const DWORD modifierState,
                                                 InputEventQueue& input)
{
    input.reserve(input.size() + 2);

    const auto sc = gsl::narrow_cast<WORD>(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC));
    auto rec = SynthesizeKeyEvent(true, 1, vkey, sc, wch, modifierState);

    input.push_back(rec);
    rec.Event.KeyEvent.bKeyDown = FALSE;
    input.push_back(rec);
}

// Method Description:
// - Writes a sequence of keypresses to the input callback based on the wch,
//      vkey and modifiers passed in. Will create both the appropriate key downs
//      and ups for that key for writing to the input. Will also generate
//      keypresses for pressing the modifier keys while typing that character.
// Arguments:
// - wch - the character to write to the input callback.
// - vkey - the VKEY of the key to write to the input callback.
// - modifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
bool InputStateMachineEngine::_WriteSingleKey(const wchar_t wch, const short vkey, const DWORD modifierState)
{
    // At most 8 records - 2 for each of shift,ctrl,alt up and down, and 2 for the actual key up and down.
    InputEventQueue inputEvents;
    _GenerateWrappedSequence(wch, vkey, modifierState, inputEvents);
    return _pDispatch->WriteInput(inputEvents);
}

// Method Description:
// - Helper for writing a single key to the input when you only know the vkey.
//      Will automatically get the wchar_t associated with that vkey.
// Arguments:
// - vkey - the VKEY of the key to write to the input callback.
// - modifierState - the modifier state to write with the key.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
bool InputStateMachineEngine::_WriteSingleKey(const short vkey, const DWORD modifierState)
{
    const auto wch = gsl::narrow_cast<wchar_t>(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));
    return _WriteSingleKey(wch, vkey, modifierState);
}

// Method Description:
// - Writes a Mouse Event Record to the input callback based on the state of the mouse.
// Arguments:
// - column - the X/Column position on the viewport (0 = left-most)
// - line - the Y/Line/Row position on the viewport (0 = top-most)
// - buttonState - the mouse buttons that are being modified
// - modifierState - the modifier state to write mouse record.
// - eventFlags - the type of mouse event to write to the mouse record.
// Return Value:
// - true iff we successfully wrote the keypress to the input callback.
bool InputStateMachineEngine::_WriteMouseEvent(const til::point uiPos, const DWORD buttonState, const DWORD controlKeyState, const DWORD eventFlags)
{
    const auto rgInput = SynthesizeMouseEvent(uiPos, buttonState, controlKeyState, eventFlags);
    return _pDispatch->WriteInput({ &rgInput, 1 });
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a cursor keys
//      sequence. This is for Arrow keys, Home, End, etc.
// Arguments:
// - parameters - the set of parameters to get the modifier state from.
// - id - the identifier for the sequence we're operating on.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetCursorKeysModifierState(const VTParameters parameters, const VTID id) noexcept
{
    auto modifiers = _GetModifier(parameters.at(1));

    // Enhanced Keys (from https://docs.microsoft.com/en-us/windows/console/key-event-record-str):
    //   Enhanced keys for the IBM 101- and 102-key keyboards are the INS, DEL,
    //   HOME, END, PAGE UP, PAGE DOWN, and direction keys in the clusters to the left
    //   of the keypad; and the divide (/) and ENTER keys in the keypad.
    // This snippet detects the direction keys + HOME + END
    // actionCode should be one of the above, so just make sure it's not a CSI_F# code
    if (id < CsiActionCodes::CSI_F1 || id > CsiActionCodes::CSI_F4)
    {
        WI_SetFlag(modifiers, ENHANCED_KEY);
    }

    return modifiers;
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a "Generic"
//      keypress - one who's sequence is terminated with a '~'.
// Arguments:
// - parameters - the set of parameters to get the modifier state from.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetGenericKeysModifierState(const VTParameters parameters) noexcept
{
    auto modifiers = _GetModifier(parameters.at(1));

    // Enhanced Keys (from https://docs.microsoft.com/en-us/windows/console/key-event-record-str):
    //   Enhanced keys for the IBM 101- and 102-key keyboards are the INS, DEL,
    //   HOME, END, PAGE UP, PAGE DOWN, and direction keys in the clusters to the left
    //   of the keypad; and the divide (/) and ENTER keys in the keypad.
    // This snippet detects the non-direction keys
    const GenericKeyIdentifiers identifier = parameters.at(0);
    if (identifier <= GenericKeyIdentifiers::Next)
    {
        modifiers = WI_SetFlag(modifiers, ENHANCED_KEY);
    }

    return modifiers;
}

// Method Description:
// - Retrieves the modifier state from the first parameter of an SGR
//      Mouse Sequence - one who's sequence is terminated with an 'M' or 'm'.
// Arguments:
// - modifierParam - the first parameter to get the modifier state from.
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetSGRMouseModifierState(const size_t modifierParam) noexcept
{
    DWORD modifiers = 0;
    // The first parameter of mouse events is encoded as the following two bytes:
    // BBDM'MMBB
    // Where each of the bits mean the following
    //   BB__'__BB - which button was pressed/released
    //   MMM - Control, Alt, Shift state (respectively)
    //   D - flag signifying a drag event
    // This retrieves the modifier state from bits [5..3] ('M' above)
    WI_SetFlagIf(modifiers, SHIFT_PRESSED, WI_IsFlagSet(modifierParam, CsiMouseModifierCodes::Shift));
    WI_SetFlagIf(modifiers, LEFT_ALT_PRESSED, WI_IsFlagSet(modifierParam, CsiMouseModifierCodes::Meta));
    WI_SetFlagIf(modifiers, LEFT_CTRL_PRESSED, WI_IsFlagSet(modifierParam, CsiMouseModifierCodes::Ctrl));
    return modifiers;
}

// Method Description:
// - Converts a VT encoded modifier param into a INPUT_RECORD compatible one.
// Arguments:
// - modifierParam - the VT modifier value to convert
// Return Value:
// - The equivalent INPUT_RECORD modifier value.
DWORD InputStateMachineEngine::_GetModifier(const size_t modifierParam) noexcept
{
    // VT Modifiers are 1+(modifier flags)
    const auto vtParam = modifierParam - 1;
    DWORD modifierState = 0;
    WI_SetFlagIf(modifierState, SHIFT_PRESSED, WI_IsFlagSet(vtParam, VT_SHIFT));
    WI_SetFlagIf(modifierState, LEFT_ALT_PRESSED, WI_IsFlagSet(vtParam, VT_ALT));
    WI_SetFlagIf(modifierState, LEFT_CTRL_PRESSED, WI_IsFlagSet(vtParam, VT_CTRL));
    return modifierState;
}

// Method Description:
// - Synthesize the button state for the Mouse Input Record from an SGR VT Sequence
// - Here, we refer to and maintain the global state of our mouse.
// - Mouse wheel events are added at the end to keep them out of the global state
// Arguments:
// - id: the sequence identifier representing whether the button was pressed or released
// - sgrEncoding: the first parameter, encoding the button and drag state
// - buttonState: Receives the button state for the record
// - eventFlags: Receives the special mouse events for the record
// Return Value:
// true iff we were able to synthesize buttonState
bool InputStateMachineEngine::_UpdateSGRMouseButtonState(const VTID id,
                                                         const size_t sgrEncoding,
                                                         DWORD& buttonState,
                                                         DWORD& eventFlags,
                                                         const til::point uiPos)
{
    // Starting with the state from the last mouse event we received
    buttonState = _mouseButtonState;
    eventFlags = 0;

    // The first parameter of mouse events is encoded as the following two bytes:
    // BBDM'MMBB
    // Where each of the bits mean the following
    //   BB__'__BB - which button was pressed/released
    //   MMM - Control, Alt, Shift state (respectively)
    //   D - flag signifying a drag event

    // This retrieves the 2 MSBs and concatenates them to the 2 LSBs to create BBBB in binary
    // This represents which button had a change in state
    const auto buttonID = (sgrEncoding & 0x3) | ((sgrEncoding & 0xC0) >> 4);
    const auto currentTime = std::chrono::steady_clock::now();
    // Step 1: Translate which button was affected
    // NOTE: if scrolled, having buttonFlag = 0 means
    //       we don't actually update the buttonState
    DWORD buttonFlag = 0;
    switch (buttonID)
    {
    case CsiMouseButtonCodes::Left:
        buttonFlag = FROM_LEFT_1ST_BUTTON_PRESSED;
        break;
    case CsiMouseButtonCodes::Right:
        buttonFlag = RIGHTMOST_BUTTON_PRESSED;
        break;
    case CsiMouseButtonCodes::Middle:
        buttonFlag = FROM_LEFT_2ND_BUTTON_PRESSED;
        break;
    case CsiMouseButtonCodes::ScrollBack:
    {
        // set high word to proper scroll direction
        // scroll intensity is assumed to be constant value
        buttonState |= SCROLL_DELTA_BACKWARD;
        eventFlags |= MOUSE_WHEELED;
        break;
    }
    case CsiMouseButtonCodes::ScrollForward:
    {
        // set high word to proper scroll direction
        // scroll intensity is assumed to be constant value
        buttonState |= SCROLL_DELTA_FORWARD;
        eventFlags |= MOUSE_WHEELED;
        break;
    }
    case CsiMouseButtonCodes::Released:
        // hover event, we still want to send these but we don't
        // need to do anything special here, so just break
        break;
    default:
        // no detectable buttonID, so we can't update the state
        return false;
    }

    // Step 2: Decide whether to set or clear that button's bit
    // NOTE: WI_SetFlag/WI_ClearFlag can't be used here because buttonFlag would have to be a compile-time constant
    switch (id)
    {
    case CsiActionCodes::MouseDown:
        // set flag
        // NOTE: scroll events have buttonFlag = 0
        //       so this intentionally does nothing
        buttonState |= buttonFlag;
        // Check if this mouse down is a double click
        // and also update our trackers for last clicked position, time and button
        if (_lastMouseClickPos && _lastMouseClickTime && _lastMouseClickButton &&
            uiPos == _lastMouseClickPos &&
            (currentTime - _lastMouseClickTime.value()) < _doubleClickTime &&
            buttonID == _lastMouseClickButton)
        {
            // This was a double click, set the flag and reset our trackers
            // for last clicked position, time and button (this is so we don't send
            // another double click on a third click)
            eventFlags |= DOUBLE_CLICK;
            _lastMouseClickPos.reset();
            _lastMouseClickTime.reset();
            _lastMouseClickButton.reset();
        }
        else if (buttonID == CsiMouseButtonCodes::Left ||
                 buttonID == CsiMouseButtonCodes::Right ||
                 buttonID == CsiMouseButtonCodes::Middle)
        {
            // This was a single click, update our trackers for last
            // clicked position and time
            _lastMouseClickPos = uiPos;
            _lastMouseClickTime = currentTime;
            _lastMouseClickButton = buttonID;
        }
        break;
    case CsiActionCodes::MouseUp:
        // clear flag
        buttonState &= (~buttonFlag);
        break;
    default:
        // no detectable change of state, so we can't update the state
        return false;
    }

    // Step 3: check if mouse moved
    if (WI_IsFlagSet(sgrEncoding, CsiMouseModifierCodes::Drag))
    {
        eventFlags |= MOUSE_MOVED;
    }

    // Step 4: update internal state before returning, even if we couldn't fully understand this
    // only take LOWORD here because HIWORD is reserved for mouse wheel delta and release events for the wheel buttons are not reported
    _mouseButtonState = LOWORD(buttonState);

    return true;
}

// Method Description:
// - Gets the Vkey form the generic keys table associated with a particular
//   identifier code.
// Arguments:
// - identifier: the identifier of the key we're looking for.
// - vkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetGenericVkey(const GenericKeyIdentifiers identifier, short& vkey) const
{
    vkey = 0;

    const auto mapping = std::find(s_genericMap.cbegin(), s_genericMap.cend(), identifier);
    if (mapping != s_genericMap.end())
    {
        vkey = mapping->vkey;
        return true;
    }

    return false;
}

// Method Description:
// - Gets the Vkey from the CSI codes table associated with a particular character.
// Arguments:
// - id: the sequence identifier to get the mapped vkey of.
// - vkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetCursorKeysVkey(const VTID id, short& vkey) const
{
    vkey = 0;

    const auto mapping = std::find(s_csiMap.cbegin(), s_csiMap.cend(), id);
    if (mapping != s_csiMap.end())
    {
        vkey = mapping->vkey;
        return true;
    }

    return false;
}

// Method Description:
// - Gets the Vkey from the SS3 codes table associated with a particular character.
// Arguments:
// - wch: the wchar_t to get the mapped vkey of.
// - pVkey: Receives the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetSs3KeysVkey(const wchar_t wch, short& vkey) const
{
    vkey = 0;

    const auto mapping = std::find(s_ss3Map.cbegin(), s_ss3Map.cend(), (Ss3ActionCodes)wch);
    if (mapping != s_ss3Map.end())
    {
        vkey = mapping->vkey;
        return true;
    }

    return false;
}

// Method Description:
// - Gets the Vkey and modifier state that's associated with a particular char.
// Arguments:
// - wch: the wchar_t to get the vkey and modifier state of.
// - vkey: Receives the vkey
// - modifierState: Receives the modifier state
// Return Value:
// <none>
bool InputStateMachineEngine::_GenerateKeyFromChar(const wchar_t wch,
                                                   short& vkey,
                                                   DWORD& modifierState) noexcept
{
    // Low order byte is key, high order is modifiers
    const auto keyscan = OneCoreSafeVkKeyScanW(wch);

    short key = LOBYTE(keyscan);

    const short keyscanModifiers = HIBYTE(keyscan);

    if (key == -1 && keyscanModifiers == -1)
    {
        return false;
    }

    // Because of course, these are not the same flags.
    short modifierFlags = 0 |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_SHIFT) ? SHIFT_PRESSED : 0) |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_CTRL) ? LEFT_CTRL_PRESSED : 0) |
                          (WI_IsFlagSet(keyscanModifiers, KEYSCAN_ALT) ? LEFT_ALT_PRESSED : 0);

    vkey = key;
    modifierState = modifierFlags;

    return true;
}

// Method Description:
// - Sets us up for vt input passthrough.
//      We'll set a couple members, and if they aren't null, when we get a
//      sequence we don't understand, we'll pass it along to the app
//      instead of eating it ourselves.
// Arguments:
// - pfnFlushToInputQueue: This is a callback to the underlying state machine to
//      trigger it to call ActionPassThroughString with whatever sequence it's
//      currently processing.
// Return Value:
// - <none>
void InputStateMachineEngine::SetFlushToInputQueueCallback(std::function<bool()> pfnFlushToInputQueue)
{
    _pfnFlushToInputQueue = pfnFlushToInputQueue;
}

// Method Description:
// - Retrieves the type of window manipulation operation from the parameter pool
//      stored during Param actions.
//  This is kept separate from the output version, as there may be
//      codes that are supported in one direction but not the other.
// Arguments:
// - parameters - Array of parameters collected
// - function - Receives the function type
// Return Value:
// - True iff we successfully pulled the function type from the parameters
bool InputStateMachineEngine::_GetWindowManipulationType(const std::span<const size_t> parameters,
                                                         unsigned int& function) const noexcept
{
    auto success = false;
    function = DispatchTypes::WindowManipulationType::Invalid;

    if (!parameters.empty())
    {
        switch (til::at(parameters, 0))
        {
        case DispatchTypes::WindowManipulationType::RefreshWindow:
            function = DispatchTypes::WindowManipulationType::RefreshWindow;
            success = true;
            break;
        case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
            function = DispatchTypes::WindowManipulationType::ResizeWindowInCharacters;
            success = true;
            break;
        default:
            success = false;
        }
    }

    return success;
}

// Method Description:
// - Attempt to parse our parameters into a win32-input-mode serialized KeyEvent.
// Arguments:
// - parameters: the list of numbers to parse into values for the KeyEvent.
// Return Value:
// - The deserialized KeyEvent.
INPUT_RECORD InputStateMachineEngine::_GenerateWin32Key(const VTParameters& parameters)
{
    // Sequences are formatted as follows:
    //
    // ^[ [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    //
    //      Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.
    //      Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.
    //      Uc: the decimal value of UnicodeChar - for example, NUL is "0", LF is
    //          "10", the character 'A' is "65". If omitted, defaults to '0'.
    //      Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.
    //      Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.
    //      Rc: the value of wRepeatCount - any number. If omitted, defaults to '1'.
    return SynthesizeKeyEvent(
        parameters.at(3).value_or(0),
        ::base::saturated_cast<uint16_t>(parameters.at(5).value_or(1)),
        ::base::saturated_cast<uint16_t>(parameters.at(0).value_or(0)),
        ::base::saturated_cast<uint16_t>(parameters.at(1).value_or(0)),
        ::base::saturated_cast<wchar_t>(parameters.at(2).value_or(0)),
        ::base::saturated_cast<uint32_t>(parameters.at(4).value_or(0)));
}
