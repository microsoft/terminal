// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "..\precomp.h"
#include <windows.h>
#include <wextestclass.h>
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\..\input\terminalInput.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\..\..\interactivity\inc\VtApiRedirection.hpp"
#endif

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

// For magic reasons, this has to live outside the class. Something wonderful about TAEF macros makes it
// invisible to the linker when inside the class.
static std::wstring s_expectedInput;

class Microsoft::Console::VirtualTerminal::InputTest
{
public:
    TEST_CLASS(InputTest);

    static void s_TerminalInputTestCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    static void s_TerminalInputTestNullCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);

    TEST_METHOD(TerminalInputTests);
    TEST_METHOD(TerminalInputModifierKeyTests);
    TEST_METHOD(TerminalInputNullKeyTests);
    TEST_METHOD(DifferentModifiersTest);
    TEST_METHOD(CtrlNumTest);

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

void InputTest::s_TerminalInputTestCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    auto records = IInputEvent::ToInputRecords(inEvents);

    if (VERIFY_ARE_EQUAL(s_expectedInput.size(), records.size(), L"Verify expected and actual input array lengths matched."))
    {
        Log::Comment(L"We are expecting always key events and always key down. All other properties should not be written by simulated keys.");

        INPUT_RECORD irExpected = { 0 };
        irExpected.EventType = KEY_EVENT;
        irExpected.Event.KeyEvent.bKeyDown = TRUE;
        irExpected.Event.KeyEvent.wRepeatCount = 1;

        Log::Comment(L"Verifying individual array members...");
        for (size_t i = 0; i < records.size(); i++)
        {
            irExpected.Event.KeyEvent.uChar.UnicodeChar = s_expectedInput[i];
            VERIFY_ARE_EQUAL(irExpected, records[i], NoThrowString().Format(L"%c, %c", s_expectedInput[i], records[i].Event.KeyEvent.uChar.UnicodeChar));
        }
    }
}

void InputTest::s_TerminalInputTestNullCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    auto records = IInputEvent::ToInputRecords(inEvents);

    if (records.size() == 1)
    {
        Log::Comment(L"We are expecting a null input event.");

        INPUT_RECORD irExpected = { 0 };
        irExpected.EventType = KEY_EVENT;
        irExpected.Event.KeyEvent.bKeyDown = TRUE;
        irExpected.Event.KeyEvent.wRepeatCount = 1;
        irExpected.Event.KeyEvent.wVirtualKeyCode = LOBYTE(VkKeyScanW(0));
        irExpected.Event.KeyEvent.dwControlKeyState = LEFT_CTRL_PRESSED;
        irExpected.Event.KeyEvent.wVirtualScanCode = 0;
        irExpected.Event.KeyEvent.uChar.UnicodeChar = L'\x0';

        VERIFY_ARE_EQUAL(irExpected, records[0]);
    }
    else if (records.size() == 2)
    {
        Log::Comment(L"We are expecting a null input event, preceded by an escape");

        INPUT_RECORD irExpectedEscape = { 0 };
        irExpectedEscape.EventType = KEY_EVENT;
        irExpectedEscape.Event.KeyEvent.bKeyDown = TRUE;
        irExpectedEscape.Event.KeyEvent.wRepeatCount = 1;
        irExpectedEscape.Event.KeyEvent.wVirtualKeyCode = 0;
        irExpectedEscape.Event.KeyEvent.dwControlKeyState = 0;
        irExpectedEscape.Event.KeyEvent.wVirtualScanCode = 0;
        irExpectedEscape.Event.KeyEvent.uChar.UnicodeChar = L'\x1b';

        INPUT_RECORD irExpected = { 0 };
        irExpected.EventType = KEY_EVENT;
        irExpected.Event.KeyEvent.bKeyDown = TRUE;
        irExpected.Event.KeyEvent.wRepeatCount = 1;
        irExpected.Event.KeyEvent.wVirtualKeyCode = 0;
        irExpected.Event.KeyEvent.dwControlKeyState = 0;
        irExpected.Event.KeyEvent.wVirtualScanCode = 0;
        irExpected.Event.KeyEvent.uChar.UnicodeChar = L'\x0';

        VERIFY_ARE_EQUAL(irExpectedEscape, records[0]);
        VERIFY_ARE_EQUAL(irExpected, records[1]);
    }
    else
    {
        VERIFY_FAIL(NoThrowString().Format(L"Expected either one or two inputs, got %zu", records.size()));
    }
}

void InputTest::TerminalInputTests()
{
    Log::Comment(L"Starting test...");

    TerminalInput* const pInput = new TerminalInput(s_TerminalInputTestCallback);

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");
    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        Log::Comment(NoThrowString().Format(L"Testing Key 0x%x", vkey));

        bool fExpectedKeyHandled = true;

        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = LOWORD(MapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));

        // Set up expected result
        switch (vkey)
        {
        case VK_TAB:
            s_expectedInput = L"\x09";
            break;
        case VK_BACK:
            s_expectedInput = L"\x7f";
            break;
        case VK_ESCAPE:
            s_expectedInput = L"\x1b";
            break;
        case VK_PAUSE:
            s_expectedInput = L"\x1a";
            break;
        case VK_UP:
            s_expectedInput = L"\x1b[A";
            break;
        case VK_DOWN:
            s_expectedInput = L"\x1b[B";
            break;
        case VK_RIGHT:
            s_expectedInput = L"\x1b[C";
            break;
        case VK_LEFT:
            s_expectedInput = L"\x1b[D";
            break;
        case VK_HOME:
            s_expectedInput = L"\x1b[H";
            break;
        case VK_INSERT:
            s_expectedInput = L"\x1b[2~";
            break;
        case VK_DELETE:
            s_expectedInput = L"\x1b[3~";
            break;
        case VK_END:
            s_expectedInput = L"\x1b[F";
            break;
        case VK_PRIOR:
            s_expectedInput = L"\x1b[5~";
            break;
        case VK_NEXT:
            s_expectedInput = L"\x1b[6~";
            break;
        case VK_F1:
            s_expectedInput = L"\x1bOP";
            break;
        case VK_F2:
            s_expectedInput = L"\x1bOQ";
            break;
        case VK_F3:
            s_expectedInput = L"\x1bOR";
            break;
        case VK_F4:
            s_expectedInput = L"\x1bOS";
            break;
        case VK_F5:
            s_expectedInput = L"\x1b[15~";
            break;
        case VK_F6:
            s_expectedInput = L"\x1b[17~";
            break;
        case VK_F7:
            s_expectedInput = L"\x1b[18~";
            break;
        case VK_F8:
            s_expectedInput = L"\x1b[19~";
            break;
        case VK_F9:
            s_expectedInput = L"\x1b[20~";
            break;
        case VK_F10:
            s_expectedInput = L"\x1b[21~";
            break;
        case VK_F11:
            s_expectedInput = L"\x1b[23~";
            break;
        case VK_F12:
            s_expectedInput = L"\x1b[24~";
            break;
        case VK_CANCEL:
            s_expectedInput = L"\x3";
            break;
        default:
            fExpectedKeyHandled = false;
            break;
        }
        if (!fExpectedKeyHandled && irTest.Event.KeyEvent.uChar.UnicodeChar != 0)
        {
            s_expectedInput.clear();
            s_expectedInput.push_back(irTest.Event.KeyEvent.uChar.UnicodeChar);
            fExpectedKeyHandled = true;
        }
        auto inputEvent = IInputEvent::Create(irTest);
        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");
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

        auto inputEvent = IInputEvent::Create(irTest);
        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(false, pInput->HandleKey(inputEvent.get()), L"Verify key was NOT handled.");
    }

    Log::Comment(L"Verify other types of events are not handled/intercepted.");

    INPUT_RECORD irUnhandled = { 0 };

    Log::Comment(L"Testing MOUSE_EVENT");
    irUnhandled.EventType = MOUSE_EVENT;
    auto inputEvent = IInputEvent::Create(irUnhandled);
    VERIFY_ARE_EQUAL(false, pInput->HandleKey(inputEvent.get()), L"Verify MOUSE_EVENT was NOT handled.");

    Log::Comment(L"Testing WINDOW_BUFFER_SIZE_EVENT");
    irUnhandled.EventType = WINDOW_BUFFER_SIZE_EVENT;
    inputEvent = IInputEvent::Create(irUnhandled);
    VERIFY_ARE_EQUAL(false, pInput->HandleKey(inputEvent.get()), L"Verify WINDOW_BUFFER_SIZE_EVENT was NOT handled.");

    Log::Comment(L"Testing MENU_EVENT");
    irUnhandled.EventType = MENU_EVENT;
    inputEvent = IInputEvent::Create(irUnhandled);
    VERIFY_ARE_EQUAL(false, pInput->HandleKey(inputEvent.get()), L"Verify MENU_EVENT was NOT handled.");

    Log::Comment(L"Testing FOCUS_EVENT");
    irUnhandled.EventType = FOCUS_EVENT;
    inputEvent = IInputEvent::Create(irUnhandled);
    VERIFY_ARE_EQUAL(false, pInput->HandleKey(inputEvent.get()), L"Verify FOCUS_EVENT was NOT handled.");
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

    TerminalInput* const pInput = new TerminalInput(s_TerminalInputTestCallback);
    const BYTE slashVkey = LOBYTE(VkKeyScanW(L'/'));
    const BYTE nullVkey = LOBYTE(VkKeyScanW(0));

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");
    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        Log::Comment(NoThrowString().Format(L"Testing Key 0x%x", vkey));

        bool fExpectedKeyHandled = true;
        bool fModifySequence = false;
        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = LOWORD(MapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR));

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

        // Set up expected result
        switch (vkey)
        {
        case VK_BACK:
            // Backspace is kinda different from other keys - we'll handle in another test.
        case VK_OEM_2:
            // VK_OEM_2 is typically the '/?' key
            continue;
            // s_expectedInput = L"\x7f";
            break;
        case VK_PAUSE:
            s_expectedInput = L"\x1a";
            break;
        case VK_UP:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mA";
            break;
        case VK_DOWN:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mB";
            break;
        case VK_RIGHT:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mC";
            break;
        case VK_LEFT:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mD";
            break;
        case VK_HOME:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mH";
            break;
        case VK_INSERT:
            fModifySequence = true;
            s_expectedInput = L"\x1b[2;m~";
            break;
        case VK_DELETE:
            fModifySequence = true;
            s_expectedInput = L"\x1b[3;m~";
            break;
        case VK_END:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mF";
            break;
        case VK_PRIOR:
            fModifySequence = true;
            s_expectedInput = L"\x1b[5;m~";
            break;
        case VK_NEXT:
            fModifySequence = true;
            s_expectedInput = L"\x1b[6;m~";
            break;
        case VK_F1:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mP";
            break;
        case VK_F2:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mQ";
            break;
        case VK_F3:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mR";
            break;
        case VK_F4:
            fModifySequence = true;
            s_expectedInput = L"\x1b[1;mS";
            break;
        case VK_F5:
            fModifySequence = true;
            s_expectedInput = L"\x1b[15;m~";
            break;
        case VK_F6:
            fModifySequence = true;
            s_expectedInput = L"\x1b[17;m~";
            break;
        case VK_F7:
            fModifySequence = true;
            s_expectedInput = L"\x1b[18;m~";
            break;
        case VK_F8:
            fModifySequence = true;
            s_expectedInput = L"\x1b[19;m~";
            break;
        case VK_F9:
            fModifySequence = true;
            s_expectedInput = L"\x1b[20;m~";
            break;
        case VK_F10:
            fModifySequence = true;
            s_expectedInput = L"\x1b[21;m~";
            break;
        case VK_F11:
            fModifySequence = true;
            s_expectedInput = L"\x1b[23;m~";
            break;
        case VK_F12:
            fModifySequence = true;
            s_expectedInput = L"\x1b[24;m~";
            break;
        case VK_TAB:
            if (AltPressed(uiKeystate))
            {
                // Alt+Tab isn't possible - thats reserved by the system.
                continue;
            }
            else if (ShiftPressed(uiKeystate))
            {
                s_expectedInput = L"\x1b[Z";
            }
            else if (ControlPressed(uiKeystate))
            {
                s_expectedInput = L"\t";
            }
            break;
        default:
            wchar_t ch = irTest.Event.KeyEvent.uChar.UnicodeChar;

            // Alt+Key generates [0x1b, Ctrl+key] into the stream
            // Pressing the control key causes all bits but the 5 least
            // significant ones to be zeroed out (when using ASCII).
            if (AltPressed(uiKeystate) && ControlPressed(uiKeystate) && ch > 0x40 && ch <= 0x5A)
            {
                s_expectedInput.clear();
                s_expectedInput.push_back(L'\x1b');
                s_expectedInput.push_back(ch & 0b11111);
                break;
            }

            // Alt+Key generates [0x1b, key] into the stream
            if (AltPressed(uiKeystate) && !ControlPressed(uiKeystate) && ch != 0)
            {
                s_expectedInput.clear();
                s_expectedInput.push_back(L'\x1b');
                s_expectedInput.push_back(ch);
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
                s_expectedInput.clear();
                s_expectedInput.push_back(irTest.Event.KeyEvent.uChar.UnicodeChar);
                break;
            }

            fExpectedKeyHandled = false;
            break;
        }

        if (fModifySequence && s_expectedInput.size() > 1)
        {
            bool fShift = !!(uiKeystate & SHIFT_PRESSED);
            bool fAlt = (uiKeystate & LEFT_ALT_PRESSED) || (uiKeystate & RIGHT_ALT_PRESSED);
            bool fCtrl = (uiKeystate & LEFT_CTRL_PRESSED) || (uiKeystate & RIGHT_CTRL_PRESSED);
            s_expectedInput[s_expectedInput.size() - 2] = L'1' + (fShift ? 1 : 0) + (fAlt ? 2 : 0) + (fCtrl ? 4 : 0);
        }

        Log::Comment(NoThrowString().Format(L"Expected = \"%s\"", s_expectedInput.c_str()));

        auto inputEvent = IInputEvent::Create(irTest);
        // Send key into object (will trigger callback and verification)
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");
    }
}

void InputTest::TerminalInputNullKeyTests()
{
    Log::Comment(L"Starting test...");

    unsigned int uiKeystate = LEFT_CTRL_PRESSED;

    TerminalInput* const pInput = new TerminalInput(s_TerminalInputTestNullCallback);

    Log::Comment(L"Sending every possible VKEY at the input stream for interception during key DOWN.");

    BYTE vkey = '2';
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));

    INPUT_RECORD irTest = { 0 };
    irTest.EventType = KEY_EVENT;
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    irTest.Event.KeyEvent.wRepeatCount = 1;
    irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
    irTest.Event.KeyEvent.bKeyDown = TRUE;

    // Send key into object (will trigger callback and verification)
    auto inputEvent = IInputEvent::Create(irTest);
    VERIFY_ARE_EQUAL(true, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");

    vkey = VK_SPACE;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
    irTest.Event.KeyEvent.uChar.UnicodeChar = vkey;
    inputEvent = IInputEvent::Create(irTest);
    VERIFY_ARE_EQUAL(true, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");

    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    inputEvent = IInputEvent::Create(irTest);
    VERIFY_ARE_EQUAL(true, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");

    uiKeystate = RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    Log::Comment(NoThrowString().Format(L"Testing key, state =0x%x, 0x%x", vkey, uiKeystate));
    irTest.Event.KeyEvent.dwControlKeyState = uiKeystate;
    inputEvent = IInputEvent::Create(irTest);
    VERIFY_ARE_EQUAL(true, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");
}

void TestKey(TerminalInput* const pInput, const unsigned int uiKeystate, const BYTE vkey, const wchar_t wch = 0)
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
    auto inputEvent = IInputEvent::Create(irTest);
    VERIFY_ARE_EQUAL(true, pInput->HandleKey(inputEvent.get()), L"Verify key was handled if it should have been.");
}

void InputTest::DifferentModifiersTest()
{
    Log::Comment(L"Starting test...");

    TerminalInput* const pInput = new TerminalInput(s_TerminalInputTestCallback);

    Log::Comment(L"Sending a bunch of keystrokes that are a little weird.");

    unsigned int uiKeystate = 0;
    BYTE vkey = VK_BACK;
    s_expectedInput = L"\x7f";
    TestKey(pInput, uiKeystate, vkey);

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_BACK;
    s_expectedInput = L"\x8";
    TestKey(pInput, uiKeystate, vkey, L'\x8');
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(pInput, uiKeystate, vkey, L'\x8');

    uiKeystate = LEFT_ALT_PRESSED;
    vkey = VK_BACK;
    s_expectedInput = L"\x1b\x7f";
    TestKey(pInput, uiKeystate, vkey, L'\x8');
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(pInput, uiKeystate, vkey, L'\x8');

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_DELETE;
    s_expectedInput = L"\x1b[3;5~";
    TestKey(pInput, uiKeystate, vkey);
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(pInput, uiKeystate, vkey);

    uiKeystate = LEFT_ALT_PRESSED;
    vkey = VK_DELETE;
    s_expectedInput = L"\x1b[3;3~";
    TestKey(pInput, uiKeystate, vkey);
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(pInput, uiKeystate, vkey);

    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = VK_TAB;
    s_expectedInput = L"\t";
    TestKey(pInput, uiKeystate, vkey);
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(pInput, uiKeystate, vkey);

    uiKeystate = SHIFT_PRESSED;
    vkey = VK_TAB;
    s_expectedInput = L"\x1b[Z";
    TestKey(pInput, uiKeystate, vkey);

    // C-/ -> C-_ -> 0x1f
    uiKeystate = LEFT_CTRL_PRESSED;
    vkey = LOBYTE(VkKeyScan(L'/'));
    s_expectedInput = L"\x1f";
    TestKey(pInput, uiKeystate, vkey, L'/');
    uiKeystate = RIGHT_CTRL_PRESSED;
    TestKey(pInput, uiKeystate, vkey, L'/');

    // M-/ -> ESC /
    uiKeystate = LEFT_ALT_PRESSED;
    vkey = LOBYTE(VkKeyScan(L'/'));
    s_expectedInput = L"\x1b/";
    TestKey(pInput, uiKeystate, vkey, L'/');
    uiKeystate = RIGHT_ALT_PRESSED;
    TestKey(pInput, uiKeystate, vkey, L'/');

    // See https://github.com/microsoft/terminal/pull/4947#issuecomment-600382856
    // C-? -> DEL -> 0x7f
    Log::Comment(NoThrowString().Format(L"Checking C-?"));
    // Use SHIFT_PRESSED to force us into differentiating between '/' and '?'
    vkey = LOBYTE(VkKeyScan(L'?'));
    s_expectedInput = L"\x7f";
    TestKey(pInput, SHIFT_PRESSED | LEFT_CTRL_PRESSED, vkey, L'?');
    TestKey(pInput, SHIFT_PRESSED | RIGHT_CTRL_PRESSED, vkey, L'?');

    // C-M-/ -> 0x1b0x1f
    Log::Comment(NoThrowString().Format(L"Checking C-M-/"));
    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    vkey = LOBYTE(VkKeyScan(L'/'));
    s_expectedInput = L"\x1b\x1f";
    TestKey(pInput, LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'/');
    TestKey(pInput, RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'/');
    // LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED is skipped because that's AltGr
    TestKey(pInput, RIGHT_CTRL_PRESSED | RIGHT_ALT_PRESSED, vkey, L'/');

    // C-M-? -> 0x1b0x7f
    Log::Comment(NoThrowString().Format(L"Checking C-M-?"));
    uiKeystate = LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED;
    vkey = LOBYTE(VkKeyScan(L'?'));
    s_expectedInput = L"\x1b\x7f";
    TestKey(pInput, SHIFT_PRESSED | LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'?');
    TestKey(pInput, SHIFT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED, vkey, L'?');
    // LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED is skipped because that's AltGr
    TestKey(pInput, SHIFT_PRESSED | RIGHT_CTRL_PRESSED | RIGHT_ALT_PRESSED, vkey, L'?');
}

void InputTest::CtrlNumTest()
{
    Log::Comment(L"Starting test...");

    TerminalInput* const pInput = new TerminalInput(s_TerminalInputTestCallback);

    Log::Comment(L"Sending the various Ctrl+Num keys.");

    unsigned int uiKeystate = LEFT_CTRL_PRESSED;
    BYTE vkey = static_cast<WORD>('1');
    s_expectedInput = L"1";
    TestKey(pInput, uiKeystate, vkey);

    Log::Comment(NoThrowString().Format(
        L"Skipping Ctrl+2, since that's supposed to send NUL, and doesn't play "
        L"nicely with this test. Ctrl+2 is covered by other tests in this class."));

    vkey = static_cast<WORD>('3');
    s_expectedInput = L"\x1b";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('4');
    s_expectedInput = L"\x1c";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('5');
    s_expectedInput = L"\x1d";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('6');
    s_expectedInput = L"\x1e";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('7');
    s_expectedInput = L"\x1f";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('8');
    s_expectedInput = L"\x7f";
    TestKey(pInput, uiKeystate, vkey);

    vkey = static_cast<WORD>('9');
    s_expectedInput = L"9";
    TestKey(pInput, uiKeystate, vkey);
}
