// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\..\inc\consoletaeftemplates.hpp"

#include <memory>
#include <utility>
#include <iostream>
#include <iomanip>
#include <sstream>

#define KEY_STATE_TOGGLED (0x1)
#define KEY_STATE_PRESSED (0x80)
#define KEY_STATE_RELEASED (0x0)

#define KEY_MESSAGE_CONTEXT_CODE (0x20000000)
#define KEY_MESSAGE_UPKEY_CODE (0xC0000000)
#define SINGLE_KEY_REPEAT (0x00000001)
#define EXTENDED_KEY_FLAG (0x01000000)

#define SLEEP_WAIT_TIME (2 * 1000)
#define GERMAN_KEYBOARD_LAYOUT (MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN))

using namespace WEX::TestExecution;
using namespace WEX::Common;
using WEX::Logging::Log;

class KeyPressTests
{
    BEGIN_TEST_CLASS(KeyPressTests)
    END_TEST_CLASS()

    void TurnOffModifierKeys(HWND hwnd)
    {
        // these are taken from GetControlKeyState.
        static const WPARAM modifiers[8] = {
            VK_LMENU,
            VK_RMENU,
            VK_LCONTROL,
            VK_RCONTROL,
            VK_SHIFT,
            VK_NUMLOCK,
            VK_SCROLL,
            VK_CAPITAL
        };
        for (unsigned int i = 0; i < 8; ++i)
        {
            PostMessage(hwnd, CM_SET_KEY_STATE, modifiers[i], KEY_STATE_RELEASED);
        }
    }

    TEST_METHOD(TestContextMenuKey)
    {
        if (!OneCoreDelay::IsPostMessageWPresent())
        {
            Log::Comment(L"Injecting keys to the window message queue cannot be done on systems without a classic window message queue. Skipping.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        Log::Comment(L"Checks that the context menu key is correctly added to the input buffer.");
        Log::Comment(L"This test will fail on some keyboard layouts. Ensure you're using a QWERTY keyboard if "
                     L"you're encountering a test failure here.");

        HWND hwnd = GetConsoleWindow();
        VERIFY_IS_TRUE(!!IsWindow(hwnd));
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD events = 0;

        // flush input buffer
        FlushConsoleInputBuffer(inputHandle);
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(inputHandle, &events));
        VERIFY_ARE_EQUAL(events, 0u);

        // send context menu key event
        TurnOffModifierKeys(hwnd);
        Sleep(SLEEP_WAIT_TIME);
        UINT scanCode = MapVirtualKeyW(VK_APPS, MAPVK_VK_TO_VSC);
        PostMessageW(hwnd, WM_KEYDOWN, VK_APPS, EXTENDED_KEY_FLAG | SINGLE_KEY_REPEAT | (scanCode << 16));
        Sleep(SLEEP_WAIT_TIME);

        INPUT_RECORD expectedRecord;
        expectedRecord.EventType = KEY_EVENT;
        expectedRecord.Event.KeyEvent.uChar.UnicodeChar = 0x0;
        expectedRecord.Event.KeyEvent.bKeyDown = true;
        expectedRecord.Event.KeyEvent.dwControlKeyState = ENHANCED_KEY;
        expectedRecord.Event.KeyEvent.dwControlKeyState |= (GetKeyState(VK_NUMLOCK) & KEY_STATE_TOGGLED) ? NUMLOCK_ON : 0;
        expectedRecord.Event.KeyEvent.wRepeatCount = SINGLE_KEY_REPEAT;
        expectedRecord.Event.KeyEvent.wVirtualKeyCode = VK_APPS;
        expectedRecord.Event.KeyEvent.wVirtualScanCode = (WORD)scanCode;

        // get the input record back and test it
        INPUT_RECORD record;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInputW(inputHandle, &record, 1, &events));
        VERIFY_IS_GREATER_THAN(events, 0u);
        VERIFY_ARE_EQUAL(expectedRecord, record);
    }

    BEGIN_TEST_METHOD(TestAltGr)
        TEST_METHOD_PROPERTY(L"Ignore[@DevTest=true]", L"false")
        TEST_METHOD_PROPERTY(L"Ignore[default]", L"true")
    END_TEST_METHOD()

    TEST_METHOD(TestCoalesceSameKeyPress)
    {
        if (!OneCoreDelay::IsSendMessageWPresent())
        {
            Log::Comment(L"Injecting keys to the window message queue cannot be done on systems without a classic window message queue. Skipping.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        Log::Comment(L"Testing that key events are properly coalesced when the same key is pressed repeatedly");
        BOOL successBool;
        HWND hwnd = GetConsoleWindow();
        VERIFY_IS_TRUE(!!IsWindow(hwnd));
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD events = 0;

        // flush input buffer
        FlushConsoleInputBuffer(inputHandle);
        successBool = GetNumberOfConsoleInputEvents(inputHandle, &events);
        VERIFY_IS_TRUE(!!successBool);
        VERIFY_ARE_EQUAL(events, 0u);

        // send a bunch of 'a' keypresses to the console
        DWORD repeatCount = 1;
        const unsigned int messageSendCount = 1000;
        for (unsigned int i = 0; i < messageSendCount; ++i)
        {
            SendMessage(hwnd,
                        WM_CHAR,
                        0x41,
                        repeatCount);
        }

        // make sure the the keypresses got processed and coalesced
        events = 0;
        successBool = GetNumberOfConsoleInputEvents(inputHandle, &events);
        VERIFY_IS_TRUE(!!successBool);
        VERIFY_IS_GREATER_THAN(events, 0u, NoThrowString().Format(L"%d", events));
        std::unique_ptr<INPUT_RECORD[]> inputBuffer = std::make_unique<INPUT_RECORD[]>(1);
        PeekConsoleInput(inputHandle,
                         inputBuffer.get(),
                         1,
                         &events);
        VERIFY_ARE_EQUAL(events, 1u);
        VERIFY_ARE_EQUAL(inputBuffer[0].EventType, KEY_EVENT);
        VERIFY_ARE_EQUAL(inputBuffer[0].Event.KeyEvent.wRepeatCount, messageSendCount, NoThrowString().Format(L"%d", inputBuffer[0].Event.KeyEvent.wRepeatCount));
    }

    TEST_METHOD(TestCtrlKeyDownUp)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // VKeys for A-Z
            // See https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
            TEST_METHOD_PROPERTY(L"Data:vKey", L"{"
                                               "0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,"
                                               "0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A"
                                               "}")
        END_TEST_METHOD_PROPERTIES();

        if (!OneCoreDelay::IsSendMessageWPresent())
        {
            Log::Comment(L"Ctrl key eventing scenario can't be checked on platform without window message queuing.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        UINT vk;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"vKey", vk));

        Log::Comment(L"Testing the right number of input events is generated by Ctrl+Key press");
        BOOL successBool;
        HWND hwnd = GetConsoleWindow();
        VERIFY_IS_TRUE(!!IsWindow(hwnd));
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD events = 0;

        // Set the console to raw mode, so that it doesn't hijack any keypresses as shortcut keys
        SetConsoleMode(inputHandle, 0);

        // flush input buffer
        FlushConsoleInputBuffer(inputHandle);
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(inputHandle, &events));
        VERIFY_ARE_EQUAL(events, 0u);

        DWORD dwInMode = 0;
        GetConsoleMode(inputHandle, &dwInMode);
        Log::Comment(NoThrowString().Format(L"Mode:0x%x", dwInMode));

        UINT vkCtrl = VK_LCONTROL; // Need this instead of VK_CONTROL
        UINT uiCtrlScancode = MapVirtualKey(vkCtrl, MAPVK_VK_TO_VSC);
        // According to
        // KEY_KEYDOWN https://msdn.microsoft.com/en-us/library/windows/desktop/ms646280(v=vs.85).aspx
        // KEY_UP https://msdn.microsoft.com/en-us/library/windows/desktop/ms646281(v=vs.85).aspx
        LPARAM CtrlFlags = (LOBYTE(uiCtrlScancode) << 16) | SINGLE_KEY_REPEAT;
        LPARAM CtrlUpFlags = CtrlFlags | KEY_MESSAGE_UPKEY_CODE;

        UINT uiScancode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
        LPARAM DownFlags = (LOBYTE(uiScancode) << 16) | SINGLE_KEY_REPEAT;
        LPARAM UpFlags = DownFlags | KEY_MESSAGE_UPKEY_CODE;

        Log::Comment(NoThrowString().Format(L"Testing Ctrl+%c", vk));
        Log::Comment(NoThrowString().Format(L"DownFlags=0x%x, CtrlFlags=0x%x", DownFlags, CtrlFlags));
        Log::Comment(NoThrowString().Format(L"UpFlags=0x%x, CtrlUpFlags=0x%x", UpFlags, CtrlUpFlags));

        // Don't Use PostMessage, those events come in the wrong order.
        // Also can't use SendInput because of the whole test window backgrounding thing.
        //      It'd work locally, until you minimize the window.
        SendMessage(hwnd, WM_KEYDOWN, vkCtrl, CtrlFlags);
        SendMessage(hwnd, WM_KEYDOWN, vk, DownFlags);
        SendMessage(hwnd, WM_KEYUP, vk, UpFlags);
        SendMessage(hwnd, WM_KEYUP, vkCtrl, CtrlUpFlags);

        Sleep(50);

        events = 0;
        successBool = GetNumberOfConsoleInputEvents(inputHandle, &events);
        VERIFY_IS_TRUE(!!successBool);
        VERIFY_IS_GREATER_THAN(events, 0u, NoThrowString().Format(L"%d events found", events));

        std::unique_ptr<INPUT_RECORD[]> inputBuffer = std::make_unique<INPUT_RECORD[]>(16);
        PeekConsoleInput(inputHandle,
                         inputBuffer.get(),
                         16,
                         &events);

        for (size_t i = 0; i < events; i++)
        {
            INPUT_RECORD rc = inputBuffer[i];
            switch (rc.EventType)
            {
            case KEY_EVENT:
            {
                Log::Comment(NoThrowString().Format(
                    L"Down: %d Repeat: %d KeyCode: 0x%x ScanCode: 0x%x Char: %c (0x%x) KeyState: 0x%x",
                    rc.Event.KeyEvent.bKeyDown,
                    rc.Event.KeyEvent.wRepeatCount,
                    rc.Event.KeyEvent.wVirtualKeyCode,
                    rc.Event.KeyEvent.wVirtualScanCode,
                    rc.Event.KeyEvent.uChar.UnicodeChar != 0 ? rc.Event.KeyEvent.uChar.UnicodeChar : ' ',
                    rc.Event.KeyEvent.uChar.UnicodeChar,
                    rc.Event.KeyEvent.dwControlKeyState));

                break;
            }
            default:
                Log::Comment(NoThrowString().Format(L"Another event type was found."));
            }
        }
        VERIFY_ARE_EQUAL(events, 4u);
        VERIFY_ARE_EQUAL(inputBuffer[0].EventType, KEY_EVENT);
        VERIFY_ARE_EQUAL(inputBuffer[1].EventType, KEY_EVENT);
        VERIFY_ARE_EQUAL(inputBuffer[2].EventType, KEY_EVENT);
        VERIFY_ARE_EQUAL(inputBuffer[3].EventType, KEY_EVENT);

        FlushConsoleInputBuffer(inputHandle);
    }

    TEST_METHOD(TestMaximize)
    {
        if (!OneCoreDelay::IsSendMessageWPresent())
        {
            Log::Comment(L"Injecting keys to the window message queue cannot be done on systems without a classic window message queue. Skipping.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        const HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        const HWND hwnd = GetConsoleWindow();
        VERIFY_IS_TRUE(!!IsWindow(hwnd));

        // Need the console to be in processed input for this to work
        SetConsoleMode(inputHandle, ENABLE_PROCESSED_INPUT);
        FlushConsoleInputBuffer(inputHandle);

        LONG oldStyle = GetWindowLongW(hwnd, GWL_STYLE);
        LONG oldExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

        // According to
        // KEY_KEYDOWN https://msdn.microsoft.com/en-us/library/windows/desktop/ms646280(v=vs.85).aspx
        // KEY_UP https://msdn.microsoft.com/en-us/library/windows/desktop/ms646281(v=vs.85).aspx
        const UINT vsc = MapVirtualKey(VK_F11, MAPVK_VK_TO_VSC);
        const LPARAM F11Flags = (LOBYTE(vsc) << 16) | SINGLE_KEY_REPEAT;
        const LPARAM F11UpFlags = F11Flags | KEY_MESSAGE_UPKEY_CODE;

        // Send F11 key down and up. lParam is VirtualScanCode and RepeatCount
        SendMessage(hwnd, WM_KEYDOWN, VK_F11, F11Flags);
        SendMessage(hwnd, WM_KEYUP, VK_F11, F11UpFlags);

        LONG maxStyle = GetWindowLongW(hwnd, GWL_STYLE);
        LONG maxExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

        // Send F11 key down and up. lParam is VirtualScanCode and RepeatCount
        SendMessage(hwnd, WM_KEYDOWN, VK_F11, F11Flags);
        SendMessage(hwnd, WM_KEYUP, VK_F11, F11UpFlags);

        LONG newStyle = GetWindowLongW(hwnd, GWL_STYLE);
        LONG newExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

        // Maximize windows should not be Overlapped & have a popup
        // Extended style should have a window edge when not maximized
        VERIFY_IS_TRUE(WI_IsFlagSet(maxStyle, WS_POPUP));
        VERIFY_IS_TRUE(WI_AreAllFlagsClear(maxStyle, WS_OVERLAPPEDWINDOW));
        VERIFY_IS_TRUE(WI_IsFlagClear(maxExStyle, WS_EX_WINDOWEDGE));

        VERIFY_IS_TRUE(WI_IsFlagClear(newStyle, WS_POPUP));
        VERIFY_IS_TRUE(WI_AreAllFlagsSet(newStyle, WS_OVERLAPPEDWINDOW));
        VERIFY_IS_TRUE(WI_IsFlagSet(newExStyle, WS_EX_WINDOWEDGE));

        VERIFY_ARE_NOT_EQUAL(maxStyle, oldStyle);
        VERIFY_ARE_NOT_EQUAL(maxExStyle, oldExStyle);

        // Ignore the scrollbars when comparing styles
        WI_ClearAllFlags(oldStyle, WS_HSCROLL | WS_VSCROLL);
        WI_ClearAllFlags(newStyle, WS_HSCROLL | WS_VSCROLL);
        VERIFY_ARE_EQUAL(oldStyle, newStyle);
        VERIFY_ARE_EQUAL(oldExStyle, newExStyle);
    }
};

void KeyPressTests::TestAltGr()
{
    Log::Comment(L"Checks that alt-gr behavior is maintained.");
    HWND hwnd = GetConsoleWindow();
    VERIFY_IS_TRUE(!!IsWindow(hwnd));
    HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD events = 0;

    // flush input buffer
    FlushConsoleInputBuffer(inputHandle);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(inputHandle, &events));
    VERIFY_ARE_EQUAL(events, 0u);

    // create german locale string
    std::wstringstream wss;
    wss << std::setfill(L'0') << std::setw(8) << std::hex << GERMAN_KEYBOARD_LAYOUT;
    std::wstring germanKeyboardLayoutString(wss.str());

    // save current keyboard layout
    wchar_t originalLocaleId[KL_NAMELENGTH];
    GetKeyboardLayoutName(originalLocaleId);

    // make console window the topmost window
    SetForegroundWindow(hwnd);

    // change to german keyboard layout
    PostMessage(hwnd, CM_SET_KEYBOARD_LAYOUT, std::stoi(germanKeyboardLayoutString), NULL);
    Sleep(SLEEP_WAIT_TIME);
    LoadKeyboardLayout(germanKeyboardLayoutString.c_str(), KLF_ACTIVATE);

    // turn off all modifier keys
    TurnOffModifierKeys(hwnd);

    // set right control key to be pressed
    PostMessage(hwnd, CM_SET_KEY_STATE, VK_LCONTROL, KEY_STATE_PRESSED);
    PostMessage(hwnd, CM_SET_KEY_STATE, VK_CONTROL, KEY_STATE_PRESSED);
    // set right alt to be pressed
    PostMessage(hwnd, CM_SET_KEY_STATE, VK_RMENU, KEY_STATE_PRESSED);
    PostMessage(hwnd, CM_SET_KEY_STATE, VK_MENU, KEY_STATE_PRESSED);
    Sleep(SLEEP_WAIT_TIME);

    // flush input buffer in preparation of the key event
    FlushConsoleInputBuffer(inputHandle);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(inputHandle, &events));
    VERIFY_ARE_EQUAL(events, 0u);

    // send the key event that will be turned into an '@'
    UINT scanCode = MapVirtualKey('Q', MAPVK_VK_TO_VSC);
    PostMessage(hwnd, WM_KEYDOWN, 'Q', KEY_MESSAGE_CONTEXT_CODE | SINGLE_KEY_REPEAT | (scanCode << 16));
    Sleep(SLEEP_WAIT_TIME);

    // reset the keymap
    TurnOffModifierKeys(hwnd);

    // create expected input record
    INPUT_RECORD expectedRecord;
    expectedRecord.EventType = KEY_EVENT;
    expectedRecord.Event.KeyEvent.uChar.UnicodeChar = L'@';
    expectedRecord.Event.KeyEvent.bKeyDown = true;
    expectedRecord.Event.KeyEvent.dwControlKeyState = RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED;
    expectedRecord.Event.KeyEvent.wRepeatCount = SINGLE_KEY_REPEAT;
    expectedRecord.Event.KeyEvent.wVirtualKeyCode = L'Q';
    expectedRecord.Event.KeyEvent.wVirtualScanCode = (WORD)scanCode;

    // read input records and compare
    const int maxRecordLookup = 20; // some arbitrary value to grab some records
    Log::Comment(L"Looking for input record matching:");
    Log::Comment(VerifyOutputTraits<INPUT_RECORD>::ToString(expectedRecord));
    INPUT_RECORD records[20];
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(inputHandle, records, maxRecordLookup, &events));
    VERIFY_IS_GREATER_THAN(events, 0u);
    bool successBool = false;
    // look for the expected record somewhere in the returned records
    for (unsigned int i = 0; i < events; ++i)
    {
        Log::Comment(VerifyOutputTraits<INPUT_RECORD>::ToString(records[i]));
        if (VerifyCompareTraits<INPUT_RECORD, INPUT_RECORD>::AreEqual(records[i], expectedRecord))
        {
            successBool = true;
            break;
        }
    }
    VERIFY_IS_TRUE(successBool);

    // reset the keyboard layout
    WPARAM originalLocale;
    std::wstringstream localeStringStream(originalLocaleId);
    localeStringStream >> std::hex >> originalLocale;
    PostMessage(hwnd, CM_SET_KEYBOARD_LAYOUT, originalLocale, NULL);
    LoadKeyboardLayout(originalLocaleId, KLF_ACTIVATE | KLF_SUBSTITUTE_OK);
}
