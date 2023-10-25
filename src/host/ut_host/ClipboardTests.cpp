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

#include "../../interactivity/inc/VtApiRedirection.hpp"

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
        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();

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

    std::vector<std::wstring> SetupRetrieveFromBuffers(bool fLineSelection, std::vector<til::inclusive_rect>& selection)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        // set up and try to retrieve the first 4 rows from the buffer
        const auto& screenInfo = gci.GetActiveOutputBuffer();

        selection.clear();
        selection.emplace_back(til::inclusive_rect{ 0, 0, 8, 0 });
        selection.emplace_back(til::inclusive_rect{ 0, 1, 14, 1 });
        selection.emplace_back(til::inclusive_rect{ 0, 2, 14, 2 });
        selection.emplace_back(til::inclusive_rect{ 0, 3, 8, 3 });

        const auto& buffer = screenInfo.GetTextBuffer();
        return buffer.GetText(true, fLineSelection, selection).text;
    }

#pragma prefast(push)
#pragma prefast(disable : 26006, "Specifically trying to check unterminated strings in this test.")
    TEST_METHOD(TestRetrieveFromBuffer)
    {
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        std::vector<til::inclusive_rect> selection;
        const auto text = SetupRetrieveFromBuffers(false, selection);

        // verify trailing bytes were trimmed
        // there are 2 double-byte characters in our sample string (see CommonState.hpp for sample)
        // the width is right - left
        VERIFY_ARE_EQUAL((til::CoordType)wcslen(text[0].data()), selection[0].right - selection[0].left + 1);

        // since we're not in line selection, the line should be \r\n terminated
        auto tempPtr = text[0].data();
        tempPtr += text[0].size();
        tempPtr -= 2;
        VERIFY_ARE_EQUAL(String(tempPtr), String(L"\r\n"));

        // since we're not in line selection, spaces should be trimmed from the end
        tempPtr = text[0].data();
        tempPtr += selection[0].right - selection[0].left - 2;
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

        std::vector<til::inclusive_rect> selection;
        const auto text = SetupRetrieveFromBuffers(true, selection);

        // row 2, no wrap
        // no wrap row before the end should have CR/LF
        auto tempPtr = text[2].data();
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
        auto ptr = wcsrchr(tempPtr, L' ');
        VERIFY_IS_NOT_NULL(ptr);
    }

    TEST_METHOD(CanConvertText)
    {
        static constexpr std::wstring_view input{ L"HeLlO WoRlD" };
        const auto events = Clipboard::Instance().TextToKeyEvents(input.data(), input.size());

        const auto shiftSC = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC));
        const auto shiftDown = SynthesizeKeyEvent(true, 1, VK_SHIFT, shiftSC, 0, SHIFT_PRESSED);
        const auto shiftUp = SynthesizeKeyEvent(false, 1, VK_SHIFT, shiftSC, 0, 0);

        InputEventQueue expectedEvents;

        for (auto wch : input)
        {
            const auto state = OneCoreSafeVkKeyScanW(wch);
            const auto vk = LOBYTE(state);
            const auto sc = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
            const auto shift = WI_IsFlagSet(state, 0x100);
            auto event = SynthesizeKeyEvent(true, 1, vk, sc, wch, shift ? SHIFT_PRESSED : 0);

            if (shift)
            {
                expectedEvents.push_back(shiftDown);
            }

            expectedEvents.push_back(event);
            event.Event.KeyEvent.bKeyDown = FALSE;
            expectedEvents.push_back(event);

            if (shift)
            {
                expectedEvents.push_back(shiftUp);
            }
        }

        VERIFY_ARE_EQUAL(expectedEvents.size(), events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expectedEvents[i], events[i]);
        }
    }

    TEST_METHOD(CanConvertCharsRequiringAltGr)
    {
        const std::wstring wstr = L"\x20ac"; // € char U+20AC

        const auto keyState = OneCoreSafeVkKeyScanW(wstr[0]);
        const WORD virtualKeyCode = LOBYTE(keyState);
        const auto virtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(virtualKeyCode, MAPVK_VK_TO_VSC));

        if (keyState == -1 || HIBYTE(keyState) == 0 /* no modifiers required */)
        {
            Log::Comment(L"This test only works on keyboard layouts where the Euro symbol exists and requires AltGr.");
            Log::Result(WEX::Logging::TestResults::Skipped);
            return;
        }

        auto events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                            wstr.size());

        InputEventQueue expectedEvents;
        // should be converted to:
        // 1. AltGr keydown
        // 2. € keydown
        // 3. € keyup
        // 4. AltGr keyup
        expectedEvents.push_back(SynthesizeKeyEvent(true, 1, VK_MENU, altScanCode, L'\0', (ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
        expectedEvents.push_back(SynthesizeKeyEvent(true, 1, virtualKeyCode, virtualScanCode, wstr[0], (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
        expectedEvents.push_back(SynthesizeKeyEvent(false, 1, virtualKeyCode, virtualScanCode, wstr[0], (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
        expectedEvents.push_back(SynthesizeKeyEvent(false, 1, VK_MENU, altScanCode, L'\0', ENHANCED_KEY));

        VERIFY_ARE_EQUAL(expectedEvents.size(), events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expectedEvents[i], events[i]);
        }
    }

    TEST_METHOD(CanConvertCharsOutsideKeyboardLayout)
    {
        const std::wstring wstr = L"\xbc"; // ¼ char U+00BC
        const UINT outputCodepage = CP_JAPANESE;
        ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP = outputCodepage;
        auto events = Clipboard::Instance().TextToKeyEvents(wstr.c_str(),
                                                            wstr.size());

        InputEventQueue expectedEvents;
        if constexpr (Feature_UseNumpadEventsForClipboardInput::IsEnabled())
        {
            // Inside Windows, where numpad events are enabled, this generated numpad events.
            // should be converted to:
            // 1. left alt keydown
            // 2. 1st numpad keydown
            // 3. 1st numpad keyup
            // 4. 2nd numpad keydown
            // 5. 2nd numpad keyup
            // 6. left alt keyup
            expectedEvents.push_back(SynthesizeKeyEvent(true, 1, VK_MENU, altScanCode, L'\0', LEFT_ALT_PRESSED));
            expectedEvents.push_back(SynthesizeKeyEvent(true, 1, 0x66, 0x4D, L'\0', LEFT_ALT_PRESSED));
            expectedEvents.push_back(SynthesizeKeyEvent(false, 1, 0x66, 0x4D, L'\0', LEFT_ALT_PRESSED));
            expectedEvents.push_back(SynthesizeKeyEvent(true, 1, 0x63, 0x51, L'\0', LEFT_ALT_PRESSED));
            expectedEvents.push_back(SynthesizeKeyEvent(false, 1, 0x63, 0x51, L'\0', LEFT_ALT_PRESSED));
            expectedEvents.push_back(SynthesizeKeyEvent(false, 1, VK_MENU, altScanCode, wstr[0], 0));
        }
        else
        {
            // Outside Windows, without numpad events, we just emit the key with a nonzero UnicodeChar
            expectedEvents.push_back(SynthesizeKeyEvent(true, 1, 0, 0, wstr[0], 0));
            expectedEvents.push_back(SynthesizeKeyEvent(false, 1, 0, 0, wstr[0], 0));
        }

        VERIFY_ARE_EQUAL(expectedEvents.size(), events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expectedEvents[i], events[i]);
        }
    }
};
