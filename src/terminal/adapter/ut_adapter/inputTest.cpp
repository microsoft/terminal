// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../../../interactivity/inc/VtApiRedirection.hpp"
#include "../../input/terminalInput.hpp"
#include "../types/inc/IInputEvent.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class InputTest;
        };
    };
};
using namespace Microsoft::Console::VirtualTerminal;

class Microsoft::Console::VirtualTerminal::InputTest
{
public:
    TEST_CLASS(InputTest);

    TEST_METHOD(TerminalInputTests);
    TEST_METHOD(TestFocusEvents);
    TEST_METHOD(TerminalInputModifierKeyTests);
    TEST_METHOD(TerminalInputNullKeyTests);
    TEST_METHOD(DifferentModifiersTest);
    TEST_METHOD(CtrlNumTest);
    TEST_METHOD(BackarrowKeyModeTest);
    TEST_METHOD(AutoRepeatModeTest);

    wchar_t GetModifierChar(const bool fShift, const bool fAlt, const bool fCtrl)
    {
        return L'1' + (fShift ? 1 : 0) + (fAlt ? 2 : 0) + (fCtrl ? 4 : 0);
    }

    bool ControlAndAltPressed(unsigned int uiKeystate)
    {
        return WI_IsAnyFlagSet(uiKeystate, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED) && WI_IsAnyFlagSet(uiKeystate, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
    }

    bool ControlOrAltPressed(unsigned int uiKeystate)
    {
        return WI_IsAnyFlagSet(uiKeystate, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
    }

    bool ControlPressed(unsigned int uiKeystate)
    {
        return WI_IsAnyFlagSet(uiKeystate, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
    }

    bool AltPressed(unsigned int uiKeystate)
    {
        return WI_IsAnyFlagSet(uiKeystate, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
    }

    bool ShiftPressed(unsigned int uiKeystate)
    {
        return WI_IsFlagSet(uiKeystate, SHIFT_PRESSED);
    }
};

void InputTest::TerminalInputTests()
{
    Log::Comment(L"Starting test...");

    TerminalInput input;

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");
    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        Log::Comment(NoThrowString().Format(L"Testing Key 0x%x", vkey));

        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = LOWORD(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));

        TerminalInput::OutputType expected;
        switch (vkey)
        {
        case VK_TAB:
            expected = TerminalInput::MakeOutput(L"\x09");
            break;
        case VK_BACK:
            expected = TerminalInput::MakeOutput(L"\x7f");
            break;
        case VK_ESCAPE:
            expected = TerminalInput::MakeOutput(L"\x1b");
            break;
        case VK_PAUSE:
            expected = TerminalInput::MakeOutput(L"\x1a");
            break;
        case VK_UP:
            expected = TerminalInput::MakeOutput(L"\x1b[A");
            break;
        case VK_DOWN:
            expected = TerminalInput::MakeOutput(L"\x1b[B");
            break;
        case VK_RIGHT:
            expected = TerminalInput::MakeOutput(L"\x1b[C");
            break;
        case VK_LEFT:
            expected = TerminalInput::MakeOutput(L"\x1b[D");
            break;
        case VK_HOME:
            expected = TerminalInput::MakeOutput(L"\x1b[H");
            break;
        case VK_INSERT:
            expected = TerminalInput::MakeOutput(L"\x1b[2~");
            break;
        case VK_DELETE:
            expected = TerminalInput::MakeOutput(L"\x1b[3~");
            break;
        case VK_END:
            expected = TerminalInput::MakeOutput(L"\x1b[F");
            break;
        case VK_PRIOR:
            expected = TerminalInput::MakeOutput(L"\x1b[5~");
            break;
        case VK_NEXT:
            expected = TerminalInput::MakeOutput(L"\x1b[6~");
            break;
        case VK_F1:
            expected = TerminalInput::MakeOutput(L"\x1bOP");
            break;
        case VK_F2:
            expected = TerminalInput::MakeOutput(L"\x1bOQ");
            break;
        case VK_F3:
            expected = TerminalInput::MakeOutput(L"\x1bOR");
            break;
        case VK_F4:
            expected = TerminalInput::MakeOutput(L"\x1bOS");
            break;
        case VK_F5:
            expected = TerminalInput::MakeOutput(L"\x1b[15~");
            break;
        case VK_F6:
            expected = TerminalInput::MakeOutput(L"\x1b[17~");
            break;
        case VK_F7:
            expected = TerminalInput::MakeOutput(L"\x1b[18~");
            break;
        case VK_F8:
            expected = TerminalInput::MakeOutput(L"\x1b[19~");
            break;
        case VK_F9:
            expected = TerminalInput::MakeOutput(L"\x1b[20~");
            break;
        case VK_F10:
            expected = TerminalInput::MakeOutput(L"\x1b[21~");
            break;
        case VK_F11:
            expected = TerminalInput::MakeOutput(L"\x1b[23~");
            break;
        case VK_F12:
            expected = TerminalInput::MakeOutput(L"\x1b[24~");
            break;
        case VK_CANCEL:
            expected = TerminalInput::MakeOutput(L"\x3");
            break;
        default:
            if (irTest.Event.KeyEvent.uChar.UnicodeChar != 0)
            {
                expected = TerminalInput::MakeOutput({ &irTest.Event.KeyEvent.uChar.UnicodeChar, 1 });
            }
            break;
        }

        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(expected, input.HandleKey(irTest), L"Verify key was handled if it should have been.");
    }

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key UP.");
    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        Log::Comment(NoThrowString().Format(L"Testing Key 0x%x", vkey));

        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = FALSE;

        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(irTest), L"Verify key was NOT handled.");
    }

    Log::Comment(L"Verify other types of events are not handled/intercepted.");

    INPUT_RECORD irUnhandled = { 0 };

    Log::Comment(L"Testing MOUSE_EVENT");
    irUnhandled.EventType = MOUSE_EVENT;
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(irUnhandled), L"Verify MOUSE_EVENT was NOT handled.");

    Log::Comment(L"Testing WINDOW_BUFFER_SIZE_EVENT");
    irUnhandled.EventType = WINDOW_BUFFER_SIZE_EVENT;
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(irUnhandled), L"Verify WINDOW_BUFFER_SIZE_EVENT was NOT handled.");

    Log::Comment(L"Testing MENU_EVENT");
    irUnhandled.EventType = MENU_EVENT;
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(irUnhandled), L"Verify MENU_EVENT was NOT handled.");

    // Testing FOCUS_EVENTs is handled by TestFocusEvents
}

void InputTest::TestFocusEvents()
{
    // GH#12900, #13238
    // Focus events that come in from the API should never be translated to VT sequences.
    // We're relying on the fact that the INPUT_RECORD version of the ctor is only called by the API
    TerminalInput input;

    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleFocus(false));
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleFocus(true));

    input.SetInputMode(TerminalInput::Mode::FocusEvent, true);

    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[O"), input.HandleFocus(false));
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[I"), input.HandleFocus(true));
}

void InputTest::TerminalInputModifierKeyTests()
{
    // Modifier key state values used in the method properties.
    // #define RIGHT_ALT_PRESSED     0x0001
    // #define LEFT_ALT_PRESSED      0x0002
    // #define RIGHT_CTRL_PRESSED    0x0004
    // #define LEFT_CTRL_PRESSED     0x0008
    // #define SHIFT_PRESSED         0x0010
    Log::Comment(L"Starting test...");
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x000A, 0x000C, 0x000E, 0x0010, 0x0011, 0x0012, 0x0013}")
    END_TEST_METHOD_PROPERTIES()

    unsigned int uiKeystate;
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiKeystate));

    TerminalInput input;
    const auto slashVkey = LOBYTE(OneCoreSafeVkKeyScanW(L'/'));
    const auto nullVkey = LOBYTE(OneCoreSafeVkKeyScanW(0));

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");
    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        Log::Comment(NoThrowString().Format(L"Testing Key 0x%x", vkey));

        auto fExpectedKeyHandled = true;
        auto fModifySequence = false;
        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = LOWORD(OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));

        if (ControlPressed(uiKeystate))
        {
            // For Ctrl-/ see DifferentModifiersTest.
            if (vkey == VK_DIVIDE || vkey == slashVkey)
            {
                continue;
            }

            // For Ctrl-@/Ctrl-Space see TerminalInputNullKeyTests.
            if (vkey == nullVkey || vkey == ' ')
            {
                continue;
            }
        }

        TerminalInput::OutputType expected;
        switch (vkey)
        {
        case VK_BACK:
            // Backspace is kinda different from other keys - we'll handle in another test.
        case VK_OEM_2:
            // VK_OEM_2 is typically the '/?' key
            continue;
            // expected = TerminalInput::MakeOutput(L"\x7f");
            break;
        case VK_PAUSE:
            expected = TerminalInput::MakeOutput(L"\x1a");
            break;
        case VK_UP:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mA");
            break;
        case VK_DOWN:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mB");
            break;
        case VK_RIGHT:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mC");
            break;
        case VK_LEFT:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mD");
            break;
        case VK_HOME:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mH");
            break;
        case VK_INSERT:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[2;m~");
            break;
        case VK_DELETE:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[3;m~");
            break;
        case VK_END:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mF");
            break;
        case VK_PRIOR:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[5;m~");
            break;
        case VK_NEXT:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[6;m~");
            break;
        case VK_F1:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mP");
            break;
        case VK_F2:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mQ");
            break;
        case VK_F3:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mR");
            break;
        case VK_F4:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[1;mS");
            break;
        case VK_F5:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[15;m~");
            break;
        case VK_F6:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[17;m~");
            break;
        case VK_F7:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[18;m~");
            break;
        case VK_F8:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[19;m~");
            break;
        case VK_F9:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[20;m~");
            break;
        case VK_F10:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[21;m~");
            break;
        case VK_F11:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[23;m~");
            break;
        case VK_F12:
            fModifySequence = true;
            expected = TerminalInput::MakeOutput(L"\x1b[24;m~");
            break;
        case VK_TAB:
            if (AltPressed(uiKeystate))
            {
                // Alt+Tab isn't possible - that's reserved by the system.
                continue;
            }
            else if (ShiftPressed(uiKeystate))
            {
                expected = TerminalInput::MakeOutput(L"\x1b[Z");
            }
            else if (ControlPressed(uiKeystate))
            {
                expected = TerminalInput::MakeOutput(L"\t");
            }
            break;
        default:
            auto ch = irTest.Event.KeyEvent.uChar.UnicodeChar;

            // Alt+Key generates [0x1b, Ctrl+key] into the stream
            // Pressing the control key causes all bits but the 5 least
            // significant ones to be zeroed out (when using ASCII).
            if (AltPressed(uiKeystate) && ControlPressed(uiKeystate) && ch > 0x40 && ch <= 0x5A)
            {
                const wchar_t buffer[2]{ L'\x1b', gsl::narrow_cast<wchar_t>(ch & 0b11111) };
                expected = TerminalInput::MakeOutput({ &buffer[0], 2 });
                break;
            }

            // Alt+Key generates [0x1b, key] into the stream
            if (AltPressed(uiKeystate) && !ControlPressed(uiKeystate) && ch != 0)
            {
                const wchar_t buffer[2]{ L'\x1b', ch };
                expected = TerminalInput::MakeOutput({ &buffer[0], 2 });
                break;
            }

            if (ControlPressed(uiKeystate) && (vkey >= '1' && vkey <= '9'))
            {
                // The C-# keys get translated into very specific control
                // characters that don't play nicely with this test. These keys
                // are tested in the CtrlNumTest Test instead.
                continue;
            }

            if (ch != 0)
            {
                expected = TerminalInput::MakeOutput({ &irTest.Event.KeyEvent.uChar.UnicodeChar, 1 });
                break;
            }

            fExpectedKeyHandled = false;
            break;
        }

        if (fModifySequence)
        {
            auto fShift = !!(uiKeystate & SHIFT_PRESSED);
            auto fAlt = (uiKeystate & LEFT_ALT_PRESSED) || (uiKeystate & RIGHT_ALT_PRESSED);
            auto fCtrl = (uiKeystate & LEFT_CTRL_PRESSED) || (uiKeystate & RIGHT_CTRL_PRESSED);
            auto& str = expected.value();
            str[str.size() - 2] = L'1' + (fShift ? 1 : 0) + (fAlt ? 2 : 0) + (fCtrl ? 4 : 0);
        }

        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(expected, input.HandleKey(irTest), L"Verify key was handled if it should have been.");
    }
}

void InputTest::TerminalInputNullKeyTests()
{
    using namespace std::string_view_literals;

    unsigned int uiKeystate = LEFT_CTRL_PRESSED;

    TerminalInput input;

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");

    BYTE vkey = LOBYTE(OneCoreSafeVkKeyScanW(0));
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));

    INPUT_RECORD irTest = { 0 };
    irTest.EventType = KEY_EVENT;
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    irTest.Event.KeyEvent.wRepeatCount = 1;
    irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
    irTest.Event.KeyEvent.bKeyDown = TRUE;

    // Send key into object (will trigger callback and verification)
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\0"sv), input.HandleKey(irTest), L"Verify key was handled if it should have been.");

    vkey = VK_SPACE;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
    irTest.Event.KeyEvent.uChar.UnicodeChar = vkey;
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\0"sv), input.HandleKey(irTest), L"Verify key was handled if it should have been.");

    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b\0"sv), input.HandleKey(irTest), L"Verify key was handled if it should have been.");

    uiKeystate = RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b\0"sv), input.HandleKey(irTest), L"Verify key was handled if it should have been.");
}

static void TestKey(const TerminalInput::OutputType& expected, TerminalInput& input, const unsigned int uiKeystate, const BYTE vkey, const wchar_t wch = 0)
{
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));

    INPUT_RECORD irTest = { 0 };
    irTest.EventType = KEY_EVENT;
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    irTest.Event.KeyEvent.wRepeatCount = 1;
    irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
    irTest.Event.KeyEvent.bKeyDown = TRUE;
    irTest.Event.KeyEvent.uChar.UnicodeChar = wch;

    // Send key into object (will trigger callback and verification)
    VERIFY_ARE_EQUAL(expected, input.HandleKey(irTest), L"Verify key was handled if it should have been.");
}

void InputTest::DifferentModifiersTest()
{
    Log::Comment(L"Starting test...");

    TerminalInput input;

    Log::Comment(L"Sending a bunch of keystrokes that are a little weird.");

    unsigned int uiKeystate = 0;
    BYTE vkey = VK_BACK;
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, uiKeystate, vkey);

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_BACK;
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, uiKeystate, vkey, L'\x8');
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, uiKeystate, vkey, L'\x8');

    uiKeystate = LEFT_ALT_PRESSED;
    vkey = VK_BACK;
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, uiKeystate, vkey, L'\x8');
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, uiKeystate, vkey, L'\x8');

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_DELETE;
    TestKey(TerminalInput::MakeOutput(L"\x1b[3;5~"), input, uiKeystate, vkey);
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x1b[3;5~"), input, uiKeystate, vkey);

    uiKeystate = LEFT_ALT_PRESSED;
    vkey = VK_DELETE;
    TestKey(TerminalInput::MakeOutput(L"\x1b[3;3~"), input, uiKeystate, vkey);
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x1b[3;3~"), input, uiKeystate, vkey);

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_TAB;
    TestKey(TerminalInput::MakeOutput(L"\t"), input, uiKeystate, vkey);
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\t"), input, uiKeystate, vkey);

    uiKeystate = SHIFT_PRESSED;
    vkey = VK_TAB;
    TestKey(TerminalInput::MakeOutput(L"\x1b[Z"), input, uiKeystate, vkey);

    // C-/ -> C-_ -> 0x1f
    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = LOBYTE(OneCoreSafeVkKeyScanW(L'/'));
    TestKey(TerminalInput::MakeOutput(L"\x1f"), input, uiKeystate, vkey, L'/');
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x1f"), input, uiKeystate, vkey, L'/');

    // M-/ -> ESC /
    uiKeystate = LEFT_ALT_PRESSED;
    vkey = LOBYTE(OneCoreSafeVkKeyScanW(L'/'));
    TestKey(TerminalInput::MakeOutput(L"\x1b/"), input, uiKeystate, vkey, L'/');
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(TerminalInput::MakeOutput(L"\x1b/"), input, uiKeystate, vkey, L'/');

    // See https://github.com/microsoft/terminal/pull/4947#issuecomment-600382856
    // C-? -> DEL -> 0x7f
    Log::Comment(NoThrowString().Format(L"Checking C-?"));
    // Use SHIFT_PRESSED to force us into differentiating between '/' and '?'
    vkey = LOBYTE(OneCoreSafeVkKeyScanW(L'?'));
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, SHIFT_PRESSED | LEFT_CTRL_PRESSED, vkey, L'?');
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, SHIFT_PRESSED | RIGHT_CTRL_PRESSED, vkey, L'?');

    // C-M-/ -> 0x1b0x1f
    Log::Comment(NoThrowString().Format(L"Checking C-M-/"));
    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    vkey = LOBYTE(OneCoreSafeVkKeyScanW(L'/'));
    TestKey(TerminalInput::MakeOutput(L"\x1b\x1f"), input, LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'/');
    TestKey(TerminalInput::MakeOutput(L"\x1b\x1f"), input, RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'/');
    // LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED is skipped because that's AltGr
    TestKey(TerminalInput::MakeOutput(L"\x1b\x1f"), input, RIGHT_CTRL_PRESSED | RIGHT_ALT_PRESSED, vkey, L'/');

    // C-M-? -> 0x1b0x7f
    Log::Comment(NoThrowString().Format(L"Checking C-M-?"));
    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    vkey = LOBYTE(OneCoreSafeVkKeyScanW(L'?'));
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, SHIFT_PRESSED | LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'?');
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, SHIFT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'?');
    // LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED is skipped because that's AltGr
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, SHIFT_PRESSED | RIGHT_CTRL_PRESSED | RIGHT_ALT_PRESSED, vkey, L'?');
}

void InputTest::CtrlNumTest()
{
    Log::Comment(L"Starting test...");

    TerminalInput input;

    Log::Comment(L"Sending the various Ctrl+Num keys.");

    unsigned int uiKeystate = LEFT_CTRL_PRESSED;
    BYTE vkey = static_cast<WORD>('1');
    TestKey(TerminalInput::MakeOutput(L"1"), input, uiKeystate, vkey);

    Log::Comment(NoThrowString().Format(
        L"Skipping Ctrl+2, since that's supposed to send NUL, and doesn't play "
        L"nicely with this test. Ctrl+2 is covered by other tests in this class."));

    vkey = static_cast<WORD>('3');
    TestKey(TerminalInput::MakeOutput(L"\x1b"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('4');
    TestKey(TerminalInput::MakeOutput(L"\x1c"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('5');
    TestKey(TerminalInput::MakeOutput(L"\x1d"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('6');
    TestKey(TerminalInput::MakeOutput(L"\x1e"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('7');
    TestKey(TerminalInput::MakeOutput(L"\x1f"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('8');
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, uiKeystate, vkey);

    vkey = static_cast<WORD>('9');
    TestKey(TerminalInput::MakeOutput(L"9"), input, uiKeystate, vkey);
}

void InputTest::BackarrowKeyModeTest()
{
    Log::Comment(L"Starting test...");

    TerminalInput input;
    const BYTE vkey = VK_BACK;

    Log::Comment(L"Sending backspace key combos with DECBKM enabled.");
    input.SetInputMode(TerminalInput::Mode::BackarrowKey, true);
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, 0, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, LEFT_CTRL_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, LEFT_CTRL_PRESSED | SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x8"), input, LEFT_ALT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x8"), input, LEFT_ALT_PRESSED | SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED | SHIFT_PRESSED, vkey);

    Log::Comment(L"Sending backspace key combos with DECBKM disabled.");
    input.SetInputMode(TerminalInput::Mode::BackarrowKey, false);
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, 0, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x7f"), input, SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, LEFT_CTRL_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x8"), input, LEFT_CTRL_PRESSED | SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, LEFT_ALT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x7f"), input, LEFT_ALT_PRESSED | SHIFT_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x8"), input, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED, vkey);
    TestKey(TerminalInput::MakeOutput(L"\x1b\x8"), input, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED | SHIFT_PRESSED, vkey);
}

void InputTest::AutoRepeatModeTest()
{
    static constexpr auto down = SynthesizeKeyEvent(true, 1, 'A', 0, 'A', 0);
    static constexpr auto up = SynthesizeKeyEvent(false, 1, 'A', 0, 'A', 0);
    TerminalInput input;

    Log::Comment(L"Sending repeating keypresses with DECARM disabled.");

    input.SetInputMode(TerminalInput::Mode::AutoRepeat, false);
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"A"), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput({}), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput({}), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(up));

    Log::Comment(L"Sending repeating keypresses with DECARM enabled.");

    input.SetInputMode(TerminalInput::Mode::AutoRepeat, true);
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"A"), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"A"), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"A"), input.HandleKey(down));
    VERIFY_ARE_EQUAL(TerminalInput::MakeUnhandled(), input.HandleKey(up));
}
