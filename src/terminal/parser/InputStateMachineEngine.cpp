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

enum class CsiActionCodes : wchar_t
{
    ArrowUp = L'A',
    ArrowDown = L'B',
    ArrowRight = L'C',
    ArrowLeft = L'D',
    Home = L'H',
    End = L'F',
    Generic = L'~', // Used for a whole bunch of possible keys
    CSI_F1 = L'P',
    CSI_F2 = L'Q',
    CSI_F3 = L'R', // Both F3 and DSR are on R.
    // DSR_DeviceStatusReportResponse = L'R',
    CSI_F4 = L'S',
    DTTERM_WindowManipulation = L't',
    CursorBackTab = L'Z',
};

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

static bool operator==(const CsiToVkey& pair, const CsiActionCodes code) noexcept
{
    return pair.action == code;
}

// Sequences ending in '~' use these numbers as identifiers.
enum class GenericKeyIdentifiers : unsigned short
{
    GenericHome = 1,
    Insert = 2,
    Delete = 3,
    GenericEnd = 4,
    Prior = 5, //PgUp
    Next = 6, //PgDn
    F5 = 15,
    F6 = 17,
    F7 = 18,
    F8 = 19,
    F9 = 20,
    F10 = 21,
    F11 = 23,
    F12 = 24,
};

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

enum class Ss3ActionCodes : wchar_t
{
    // The "Cursor Keys" are sometimes sent as a SS3 in "application mode"
    //  But for now we'll only accept them as Normal Mode sequences, as CSI's.
    // ArrowUp = L'A',
    // ArrowDown = L'B',
    // ArrowRight = L'C',
    // ArrowLeft = L'D',
    // Home = L'H',
    // End = L'F',
    SS3_F1 = L'P',
    SS3_F2 = L'Q',
    SS3_F3 = L'R',
    SS3_F4 = L'S',
};

struct Ss3ToVkey
{
    Ss3ActionCodes action;
    short vkey;
};

static constexpr std::array<Ss3ToVkey, 4> s_ss3Map = {
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
    _lookingForDSR(lookingForDSR)
{
    THROW_IF_NULL_ALLOC(_pDispatch.get());
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
    bool success = false;
    if (wch == UNICODE_ETX && !writeAlt)
    {
        // This is Ctrl+C, which is handled specially by the host.
        success = _pDispatch->WriteCtrlC();
    }
    else if (wch >= '\x0' && wch < '\x20')
    {
        // This is a C0 Control Character.
        // This should be translated as Ctrl+(wch+x40)
        wchar_t actualChar = wch;
        bool writeCtrl = true;

        short vkey = 0;
        DWORD modifierState = 0;

        switch (wch)
        {
        case L'\b':
            success = _GenerateKeyFromChar(wch + 0x40, vkey, modifierState);
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
    bool success = _GenerateKeyFromChar(wch, vkey, modifierState);
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
    return ActionPrintString(string);
}

// Method Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - wch - Character to dispatch.
// - intermediate - Intermediate character in the sequence, if there was one.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionEscDispatch(const wchar_t wch,
                                                const std::optional<wchar_t> /*intermediate*/)
{
    bool success = false;

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
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - intermediate - Intermediate character in the sequence, if there was one.
// - parameters - set of numeric parameters collected while pasring the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionCsiDispatch(const wchar_t wch,
                                                const std::optional<wchar_t> /*intermediate*/,
                                                const std::basic_string_view<size_t> parameters)
{
    DWORD modifierState = 0;
    short vkey = 0;
    unsigned int function = 0;
    size_t col = 0;
    size_t row = 0;

    // This is all the args after the first arg, and the count of args not including the first one.
    const auto remainingArgs = parameters.size() > 1 ? parameters.substr(1) : parameters;

    bool success = false;
    switch ((CsiActionCodes)wch)
    {
    case CsiActionCodes::Generic:
        modifierState = _GetGenericKeysModifierState(parameters);
        success = _GetGenericVkey(parameters, vkey);
        break;
    // case CsiActionCodes::DSR_DeviceStatusReportResponse:
    case CsiActionCodes::CSI_F3:
        // The F3 case is special - it shares a code with the DeviceStatusResponse.
        // If we're looking for that response, then do that, and break out.
        // Else, fall though to the _GetCursorKeysModifierState handler.
        if (_lookingForDSR)
        {
            success = true;
            success = _GetXYPosition(parameters, row, col);
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
        modifierState = _GetCursorKeysModifierState(parameters);
        success = _GetCursorKeysVkey(wch, vkey);
        break;
    case CsiActionCodes::CursorBackTab:
        modifierState = SHIFT_PRESSED;
        vkey = VK_TAB;
        success = true;
        break;
    case CsiActionCodes::DTTERM_WindowManipulation:
        success = _GetWindowManipulationType(parameters, function);
        break;
    default:
        success = false;
        break;
    }

    if (success)
    {
        switch ((CsiActionCodes)wch)
        {
        // case CsiActionCodes::DSR_DeviceStatusReportResponse:
        case CsiActionCodes::CSI_F3:
            // The F3 case is special - it shares a code with the DeviceStatusResponse.
            // If we're looking for that response, then do that, and break out.
            // Else, fall though to the _GetCursorKeysModifierState handler.
            if (_lookingForDSR)
            {
                success = _pDispatch->MoveCursor(row, col);
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
            success = _WriteSingleKey(vkey, modifierState);
            break;
        case CsiActionCodes::DTTERM_WindowManipulation:
            success = _pDispatch->WindowManipulation(static_cast<DispatchTypes::WindowManipulationType>(function),
                                                     remainingArgs);
            break;
        default:
            success = false;
            break;
        }
    }

    return success;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - parameters - set of numeric parameters collected while pasring the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool InputStateMachineEngine::ActionSs3Dispatch(const wchar_t wch,
                                                const std::basic_string_view<size_t> /*parameters*/)
{
    // Ss3 sequence keys aren't modified.
    // When F1-F4 *are* modified, they're sent as CSI sequences, not SS3's.
    DWORD dwModifierState = 0;
    short vkey = 0;

    bool success = _GetSs3KeysVkey(wch, vkey);

    if (success)
    {
        success = _WriteSingleKey(vkey, dwModifierState);
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
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dsipatch.
bool InputStateMachineEngine::ActionOscDispatch(const wchar_t /*wch*/,
                                                const size_t /*parameter*/,
                                                const std::wstring_view /*string*/)
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
                                                       std::vector<INPUT_RECORD>& input)
{
    input.reserve(input.size() + 8);

    // TODO: Reuse the clipboard functions for generating input for characters
    //       that aren't on the current keyboard.
    // MSFT:13994942

    const bool shift = WI_IsFlagSet(modifierState, SHIFT_PRESSED);
    const bool ctrl = WI_IsFlagSet(modifierState, LEFT_CTRL_PRESSED);
    const bool alt = WI_IsFlagSet(modifierState, LEFT_ALT_PRESSED);

    INPUT_RECORD next;

    DWORD currentModifiers = 0;

    if (shift)
    {
        WI_SetFlag(currentModifiers, SHIFT_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = TRUE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        input.push_back(next);
    }
    if (alt)
    {
        WI_SetFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = TRUE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        input.push_back(next);
    }
    if (ctrl)
    {
        WI_SetFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = TRUE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        input.push_back(next);
    }

    _GetSingleKeypress(wch, vkey, currentModifiers, input);

    if (ctrl)
    {
        WI_ClearFlag(currentModifiers, LEFT_CTRL_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = FALSE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_CONTROL;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        input.push_back(next);
    }
    if (alt)
    {
        WI_ClearFlag(currentModifiers, LEFT_ALT_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = FALSE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_MENU;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        input.push_back(next);
    }
    if (shift)
    {
        WI_ClearFlag(currentModifiers, SHIFT_PRESSED);
        next.EventType = KEY_EVENT;
        next.Event.KeyEvent.bKeyDown = FALSE;
        next.Event.KeyEvent.dwControlKeyState = currentModifiers;
        next.Event.KeyEvent.wRepeatCount = 1;
        next.Event.KeyEvent.wVirtualKeyCode = VK_SHIFT;
        next.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC));
        next.Event.KeyEvent.uChar.UnicodeChar = 0x0;
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
                                                 std::vector<INPUT_RECORD>& input)
{
    input.reserve(input.size() + 2);

    INPUT_RECORD rec;

    rec.EventType = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown = TRUE;
    rec.Event.KeyEvent.dwControlKeyState = modifierState;
    rec.Event.KeyEvent.wRepeatCount = 1;
    rec.Event.KeyEvent.wVirtualKeyCode = vkey;
    rec.Event.KeyEvent.wVirtualScanCode = (WORD)MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    rec.Event.KeyEvent.uChar.UnicodeChar = wch;
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
    std::vector<INPUT_RECORD> input;
    _GenerateWrappedSequence(wch, vkey, modifierState, input);
    std::deque<std::unique_ptr<IInputEvent>> inputEvents = IInputEvent::Create(gsl::make_span(input));

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
DWORD InputStateMachineEngine::_GetCursorKeysModifierState(const std::basic_string_view<size_t> parameters)
{
    // Both Cursor keys and generic keys keep their modifiers in the same index.
    return _GetGenericKeysModifierState(parameters);
}

// Method Description:
// - Retrieves the modifier state from a set of parameters for a "Generic"
//      keypress - one who's sequence is terminated with a '~'.
// Arguments:
// - rgusParams - the set of parameters to get the modifier state from.
// - cParams - the number of elements in rgusParams
// Return Value:
// - the INPUT_RECORD compatible modifier state.
DWORD InputStateMachineEngine::_GetGenericKeysModifierState(const std::basic_string_view<size_t> parameters)
{
    DWORD modifiers = 0;
    if (_IsModified(parameters.size()) && parameters.size() >= 2)
    {
        modifiers = _GetModifier(parameters.at(1));
    }
    return modifiers;
}

// Method Description:
// - Determines if a set of parameters indicates a modified keypress
// Arguments:
// - paramCount - the nummber of parameters we've collected in this sequence
// Return Value:
// - true iff the sequence is a modified sequence.
bool InputStateMachineEngine::_IsModified(const size_t paramCount)
{
    // modified input either looks like
    // \x1b[1;mA or \x1b[17;m~
    // Both have two parameters
    return paramCount == 2;
}

// Method Description:
// - Converts a VT encoded modifier param into a INPUT_RECORD compatible one.
// Arguments:
// - modifierParam - the VT modifier value to convert
// Return Value:
// - The equivalent INPUT_RECORD modifier value.
DWORD InputStateMachineEngine::_GetModifier(const size_t modifierParam)
{
    // VT Modifiers are 1+(modifier flags)
    const auto vtParam = modifierParam - 1;
    DWORD modifierState = 0;
    WI_SetFlagIf(modifierState, ENHANCED_KEY, modifierParam > 0);
    WI_SetFlagIf(modifierState, SHIFT_PRESSED, WI_IsFlagSet(vtParam, VT_SHIFT));
    WI_SetFlagIf(modifierState, LEFT_ALT_PRESSED, WI_IsFlagSet(vtParam, VT_ALT));
    WI_SetFlagIf(modifierState, LEFT_CTRL_PRESSED, WI_IsFlagSet(vtParam, VT_CTRL));
    return modifierState;
}

// Method Description:
// - Gets the Vkey form the generic keys table associated with a particular
//   identifier code. The identifier code will be the first param in rgusParams.
// Arguments:
// - parameters: an array of shorts where the first is the identifier of the key
//      we're looking for.
// - vkey: Recieves the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetGenericVkey(const std::basic_string_view<size_t> parameters, short& vkey) const
{
    vkey = 0;
    if (parameters.empty())
    {
        return false;
    }

    const auto identifier = (GenericKeyIdentifiers)parameters.front();

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
// - wch: the wchar_t to get the mapped vkey of.
// - vkey: Recieves the vkey
// Return Value:
// true iff we found the key
bool InputStateMachineEngine::_GetCursorKeysVkey(const wchar_t wch, short& vkey) const
{
    vkey = 0;

    const auto mapping = std::find(s_csiMap.cbegin(), s_csiMap.cend(), (CsiActionCodes)wch);
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
// - pVkey: Recieves the vkey
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
// - vkey: Recieves the vkey
// - modifierState: Recieves the modifier state
// Return Value:
// <none>
bool InputStateMachineEngine::_GenerateKeyFromChar(const wchar_t wch,
                                                   short& vkey,
                                                   DWORD& modifierState)
{
    // Low order byte is key, high order is modifiers
    short keyscan = VkKeyScanW(wch);

    short key = LOBYTE(keyscan);

    short keyscanModifiers = HIBYTE(keyscan);

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
// - parameters - Array of parameters collected
// - function - Receives the function type
// Return Value:
// - True iff we successfully pulled the function type from the parameters
bool InputStateMachineEngine::_GetWindowManipulationType(const std::basic_string_view<size_t> parameters,
                                                         unsigned int& function) const
{
    bool success = false;
    function = DispatchTypes::WindowManipulationType::Invalid;

    if (!parameters.empty())
    {
        switch (parameters.front())
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

// Routine Description:
// - Retrieves an X/Y coordinate pair for a cursor operation from the parameter pool stored during Param actions.
// Arguments:
// - parameters - set of numeric parameters collected while pasring the sequence.
// - line - Receives the Y/Line/Row position
// - column - Receives the X/Column position
// Return Value:
// - True if we successfully pulled the cursor coordinates from the parameters we've stored. False otherwise.
bool InputStateMachineEngine::_GetXYPosition(const std::basic_string_view<size_t> parameters,
                                             size_t& line,
                                             size_t& column) const
{
    bool success = true;
    line = DefaultLine;
    column = DefaultColumn;

    if (parameters.empty())
    {
        // Empty parameter sequences should use the default
    }
    else if (parameters.size() == 1)
    {
        // If there's only one param, leave the default for the column, and retrieve the specified row.
        line = parameters.front();
    }
    else if (parameters.size() == 2)
    {
        // If there are exactly two parameters, use them.
        line = parameters.front();
        column = parameters.back();
    }
    else
    {
        success = false;
    }

    // Distances of 0 should be changed to 1.
    if (line == 0)
    {
        line = DefaultLine;
    }

    if (column == 0)
    {
        column = DefaultColumn;
    }

    return success;
}
