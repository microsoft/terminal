// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "CommonState.hpp"

#include "globals.h"

#include "../interactivity/win32/Clipboard.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

#include "dbcs.h"

#include <cctype>

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif

#include "../../inc/consoletaeftemplates.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;

static const WORD altScanCode = 0x38;
static const WORD leftShiftScanCode = 0x2A;

class ClipboardTests
{
    TEST_CLASS(ClipboardTests);

    CommonState* m_state;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();
        m_state->PrepareGlobalInputBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalInputBuffer();
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state->FillTextBuffer();
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        return true;
    }

    const UINT cRectsSelected = 4;

    std::vector<std::wstring> SetupRetrieveFromBuffers(bool fLineSelection, std::vector<SMALL_RECT>& selection)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        // set up and try to retrieve the first 4 rows from the buffer
        const auto& screenInfo = gci.GetActiveOutputBuffer();

        selection.clear();
        selection.emplace_back(SMALL_RECT{ 0, 0, 8, 0 });
        selection.emplace_back(SMALL_RECT{ 0, 1, 14, 1 });
        selection.emplace_back(SMALL_RECT{ 0, 2, 14, 2 });
        selection.emplace_back(SMALL_RECT{ 0, 3, 8, 3 });

        const auto& buffer = screenInfo.GetTextBuffer();
        return buffer.GetText(true, fLineSelection, selection).text;
    }

#pragma prefast(push)
#pragma prefast(disable : 26006, "Specifically trying to check unterminated strings in this test.")
    TEST_METHOD(TestRetrieveFromBuffer)
    {
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        std::vector<SMALL_RECT> selection;
        const auto text = SetupRetrieveFromBuffers(false, selection);

        // verify trailing bytes were trimmed
        // there are 2 double-byte characters in our sample string (see CommonState.hpp for sample)
        // the width is right - left
        VERIFY_ARE_EQUAL((short)wcslen(text[0].data()), selection[0].Right - selection[0].Left + 1);

        // since we're not in line selection, the line should be \r\n terminated
        PCWCHAR tempPtr = text[0].data();
        tempPtr += text[0].size();
        tempPtr -= 2;
        VERIFY_ARE_EQUAL(String(tempPtr), String(L"\r\n"));

        // since we're not in line selection, spaces should be trimmed from the end
        tempPtr = text[0].data();
        tempPtr += selection[0].Right - selection[0].Left - 2;
        tempPtr++;
        VERIFY_IS_NULL(wcsrchr(tempPtr, L' '));

        // final line of selection should not contain CR/LF
        tempPtr = text[3].data();
        tempPtr += text[3].size();
        tempPtr -= 2;
        VERIFY_ARE_NOT_EQUAL(String(tempPtr), String(L"\r\n"));
    }
#pragma prefast(pop)

    TEST_METHOD(TestRetrieveLineSelectionFromBuffer)
    {
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        std::vector<SMALL_RECT> selection;
        const auto text = SetupRetrieveFromBuffers(true, selection);

        // row 2, no wrap
        // no wrap row before the end should have CR/LF
        PCWCHAR tempPtr = text[2].data();
        tempPtr += text[2].size();
        tempPtr -= 2;
        VERIFY_ARE_EQUAL(String(tempPtr), String(L"\r\n"));

        // no wrap row should trim spaces at the end
        tempPtr = text[2].data();
        VERIFY_IS_NULL(wcsrchr(tempPtr, L' '));

        // row 1, wrap
        // wrap row before the end should *not* have CR/LF
        tempPtr = text[1].data();
        tempPtr += text[1].size();
        tempPtr -= 2;
        VERIFY_ARE_NOT_EQUAL(String(tempPtr), String(L"\r\n"));

        // wrap row should have spaces at the end
        tempPtr = text[1].data();
        const wchar_t* ptr = wcsrchr(tempPtr, L' ');
        VERIFY_IS_NOT_NULL(ptr);
    }

    TEST_METHOD(CanConvertTextToInputEvents)
    {
        std::wstring wstr = L"hello world";
        std::deque<std::unique_ptr<IInputEvent>> events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                                                                wstr.size());
        VERIFY_ARE_EQUAL(wstr.size() * 2, events.size());
        IInputServices* pInputServices = ServiceLocator::LocateInputServices();
        for (wchar_t wch : wstr)
        {
            std::deque<bool> keydownPattern{ true, false };
            for (bool isKeyDown : keydownPattern)
            {
                VERIFY_ARE_EQUAL(InputEventType::KeyEvent, events.front()->EventType());
                std::unique_ptr<KeyEvent> keyEvent;
                keyEvent.reset(static_cast<KeyEvent* const>(events.front().release()));
                events.pop_front();

                const short keyState = pInputServices->VkKeyScanW(wch);
                VERIFY_ARE_NOT_EQUAL(-1, keyState);
                const WORD virtualScanCode = static_cast<WORD>(pInputServices->MapVirtualKeyW(LOBYTE(keyState), MAPVK_VK_TO_VSC));

                VERIFY_ARE_EQUAL(wch, keyEvent->GetCharData());
                VERIFY_ARE_EQUAL(isKeyDown, keyEvent->IsKeyDown());
                VERIFY_ARE_EQUAL(1, keyEvent->GetRepeatCount());
                VERIFY_ARE_EQUAL(static_cast<DWORD>(0), keyEvent->GetActiveModifierKeys());
                VERIFY_ARE_EQUAL(virtualScanCode, keyEvent->GetVirtualScanCode());
                VERIFY_ARE_EQUAL(LOBYTE(keyState), keyEvent->GetVirtualKeyCode());
            }
        }
    }

    TEST_METHOD(CanConvertUppercaseText)
    {
        std::wstring wstr = L"HeLlO WoRlD";
        size_t uppercaseCount = 0;
        for (wchar_t wch : wstr)
        {
            std::isupper(wch) ? ++uppercaseCount : 0;
        }
        std::deque<std::unique_ptr<IInputEvent>> events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                                                                wstr.size());

        VERIFY_ARE_EQUAL((wstr.size() + uppercaseCount) * 2, events.size());
        IInputServices* pInputServices = ServiceLocator::LocateInputServices();
        VERIFY_IS_NOT_NULL(pInputServices);
        for (wchar_t wch : wstr)
        {
            std::deque<bool> keydownPattern{ true, false };
            for (bool isKeyDown : keydownPattern)
            {
                Log::Comment(NoThrowString().Format(L"testing char: %C; keydown: %d", wch, isKeyDown));

                VERIFY_ARE_EQUAL(InputEventType::KeyEvent, events.front()->EventType());
                std::unique_ptr<KeyEvent> keyEvent;
                keyEvent.reset(static_cast<KeyEvent* const>(events.front().release()));
                events.pop_front();

                const short keyScanError = -1;
                const short keyState = pInputServices->VkKeyScanW(wch);
                VERIFY_ARE_NOT_EQUAL(keyScanError, keyState);
                const WORD virtualScanCode = static_cast<WORD>(pInputServices->MapVirtualKeyW(LOBYTE(keyState), MAPVK_VK_TO_VSC));

                if (std::isupper(wch))
                {
                    // uppercase letters have shift key events
                    // surrounding them, making two events per letter
                    // (and another two for the keyup)
                    VERIFY_IS_FALSE(events.empty());

                    VERIFY_ARE_EQUAL(InputEventType::KeyEvent, events.front()->EventType());
                    std::unique_ptr<KeyEvent> keyEvent2;
                    keyEvent2.reset(static_cast<KeyEvent* const>(events.front().release()));
                    events.pop_front();

                    const short keyState2 = pInputServices->VkKeyScanW(wch);
                    VERIFY_ARE_NOT_EQUAL(keyScanError, keyState2);
                    const WORD virtualScanCode2 = static_cast<WORD>(pInputServices->MapVirtualKeyW(LOBYTE(keyState2), MAPVK_VK_TO_VSC));

                    if (isKeyDown)
                    {
                        // shift then letter
                        const KeyEvent shiftDownEvent({ TRUE, 1, VK_SHIFT, leftShiftScanCode, L'\0', SHIFT_PRESSED });
                        VERIFY_ARE_EQUAL(shiftDownEvent, *keyEvent);

                        const KeyEvent expectedKeyEvent({ TRUE, 1, LOBYTE(keyState2), virtualScanCode2, wch, SHIFT_PRESSED });
                        VERIFY_ARE_EQUAL(expectedKeyEvent, *keyEvent2);
                    }
                    else
                    {
                        // letter then shift
                        const KeyEvent expectedKeyEvent({ FALSE, 1, LOBYTE(keyState), virtualScanCode, wch, SHIFT_PRESSED });
                        VERIFY_ARE_EQUAL(expectedKeyEvent, *keyEvent);

                        const KeyEvent shiftUpEvent({ FALSE, 1, VK_SHIFT, leftShiftScanCode, L'\0', 0 });
                        VERIFY_ARE_EQUAL(shiftUpEvent, *keyEvent2);
                    }
                }
                else
                {
                    const KeyEvent expectedKeyEvent({ !!isKeyDown, 1, LOBYTE(keyState), virtualScanCode, wch, 0 });
                    VERIFY_ARE_EQUAL(expectedKeyEvent, *keyEvent);
                }
            }
        }
    }

    TEST_METHOD(CanConvertCharsRequiringAltGr)
    {
        const std::wstring wstr = L"\x20ac"; // € char U+20AC

        const short keyState = VkKeyScanW(wstr[0]);
        const WORD virtualKeyCode = LOBYTE(keyState);
        const WORD virtualScanCode = static_cast<WORD>(MapVirtualKeyW(virtualKeyCode, MAPVK_VK_TO_VSC));

        if (keyState == -1 || HIBYTE(keyState) == 0 /* no modifiers required */)
        {
            Log::Comment(L"This test only works on keyboard layouts where the Euro symbol exists and requires AltGr.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        std::deque<std::unique_ptr<IInputEvent>> events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                                                                wstr.size());

        std::deque<KeyEvent> expectedEvents;
        // should be converted to:
        // 1. AltGr keydown
        // 2. € keydown
        // 3. € keyup
        // 4. AltGr keyup
        expectedEvents.push_back({ TRUE, 1, VK_MENU, altScanCode, L'\0', (ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED) });
        expectedEvents.push_back({ TRUE, 1, virtualKeyCode, virtualScanCode, wstr[0], (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED) });
        expectedEvents.push_back({ FALSE, 1, virtualKeyCode, virtualScanCode, wstr[0], (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED) });
        expectedEvents.push_back({ FALSE, 1, VK_MENU, altScanCode, L'\0', ENHANCED_KEY });

        VERIFY_ARE_EQUAL(expectedEvents.size(), events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            const KeyEvent currentKeyEvent = *reinterpret_cast<const KeyEvent* const>(events[i].get());
            VERIFY_ARE_EQUAL(expectedEvents[i], currentKeyEvent, NoThrowString().Format(L"i == %d", i));
        }
    }

    TEST_METHOD(CanConvertCharsOutsideKeyboardLayout)
    {
        const std::wstring wstr = L"\xbc"; // ¼ char U+00BC
        const UINT outputCodepage = CP_JAPANESE;
        ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP = outputCodepage;
        std::deque<std::unique_ptr<IInputEvent>> events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                                                                wstr.size());

        std::deque<KeyEvent> expectedEvents;
#ifdef __INSIDE_WINDOWS
        // Inside Windows, where numpad events are enabled, this generated numpad events.
        // should be converted to:
        // 1. left alt keydown
        // 2. 1st numpad keydown
        // 3. 1st numpad keyup
        // 4. 2nd numpad keydown
        // 5. 2nd numpad keyup
        // 6. left alt keyup
        expectedEvents.push_back({ TRUE, 1, VK_MENU, altScanCode, L'\0', LEFT_ALT_PRESSED });
        expectedEvents.push_back({ TRUE, 1, 0x66, 0x4D, L'\0', LEFT_ALT_PRESSED });
        expectedEvents.push_back({ FALSE, 1, 0x66, 0x4D, L'\0', LEFT_ALT_PRESSED });
        expectedEvents.push_back({ TRUE, 1, 0x63, 0x51, L'\0', LEFT_ALT_PRESSED });
        expectedEvents.push_back({ FALSE, 1, 0x63, 0x51, L'\0', LEFT_ALT_PRESSED });
        expectedEvents.push_back({ FALSE, 1, VK_MENU, altScanCode, wstr[0], 0 });
#else
        // Outside Windows, without numpad events, we just emit the key with a nonzero UnicodeChar
        expectedEvents.push_back({ TRUE, 1, 0, 0, wstr[0], 0 });
        expectedEvents.push_back({ FALSE, 1, 0, 0, wstr[0], 0 });
#endif

        VERIFY_ARE_EQUAL(expectedEvents.size(), events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            const KeyEvent currentKeyEvent = *reinterpret_cast<const KeyEvent* const>(events[i].get());
            VERIFY_ARE_EQUAL(expectedEvents[i], currentKeyEvent, NoThrowString().Format(L"i == %d", i));
        }
    }
};
