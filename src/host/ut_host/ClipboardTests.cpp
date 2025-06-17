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

        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalInputBuffer();
        m_state->CleanupGlobalScreenBuffer();

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

    std::pair<til::CoordType, til::CoordType> GetBufferSize()
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& screenInfo = gci.GetActiveOutputBuffer();
        const auto& buffer = screenInfo.GetTextBuffer();
        const auto bufferBounds = buffer.GetSize();
        return { bufferBounds.Width(), bufferBounds.Height() };
    }

    std::wstring SetupRetrieveFromBuffer(bool fLineSelection)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        // set up and try to retrieve the first 4 rows from the buffer
        const auto& screenInfo = gci.GetActiveOutputBuffer();
        const auto& buffer = screenInfo.GetTextBuffer();

        constexpr til::point_span selection = { { 0, 0 }, { 15, 3 } };
        const auto req = TextBuffer::CopyRequest::FromConfig(buffer, selection.start, selection.end, false, !fLineSelection, false);
        return buffer.GetPlainText(req);
    }

    TEST_METHOD(TestRetrieveBlockSelectionFromBuffer)
    {
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.

        const auto text = SetupRetrieveFromBuffer(false);

        std::wstring expectedText;

        // Block selection:
        // - Add line breaks on wrapped and non-wrapped rows.
        // - No trimming of trailing whitespace because trimming in Block
        //   selection is disabled.

        // All rows:
        // First 15 columns selected -> 7 characters (9 columns) + 6 spaces
        // Add CR/LF (except the last row)

        // row 0
        expectedText += L"AB\u304bC\u304dDE      ";
        expectedText += L"\r\n";

        // row 1
        expectedText += L"AB\u304bC\u304dDE      ";
        expectedText += L"\r\n";

        // row 2
        expectedText += L"AB\u304bC\u304dDE      ";
        expectedText += L"\r\n";

        // row 3
        // last row -> no CR/LF
        expectedText += L"AB\u304bC\u304dDE      ";

        VERIFY_ARE_EQUAL(expectedText, text);
    }

    TEST_METHOD(TestRetrieveLineSelectionFromBuffer)
    {
        // NOTE: This test requires innate knowledge of how the common buffer text is emitted in order to test all cases
        // Please see CommonState.hpp for information on the buffer state per row, the row contents, etc.
        const auto text = SetupRetrieveFromBuffer(true);

        std::wstring expectedText;

        // Line Selection:
        // - Add line breaks on non-wrapped rows.
        // - Trim trailing whitespace on non-wrapped rows.

        // row 0
        // no wrap -> trim trailing whitespace, add CR/LF
        // All columns selected -> 7 characters, trimmed trailing spaces
        expectedText += L"AB\u304bC\u304dDE";
        expectedText += L"\r\n";

        // row 1
        // wrap -> no trimming of trailing whitespace, no CR/LF
        // All columns selected -> 7 characters (9 columns) + (bufferWidth - 9) spaces
        const auto [bufferWidth, bufferHeight] = GetBufferSize();
        expectedText += L"AB\u304bC\u304dDE" + std::wstring(bufferWidth - 9, L' ');

        // row 2
        // no wrap -> trim trailing whitespace, add CR/LF
        // All columns selected -> 7 characters (9 columns), trimmed trailing spaces
        expectedText += L"AB\u304bC\u304dDE";
        expectedText += L"\r\n";

        // row 3
        // wrap -> no trimming of trailing whitespace, no CR/LF
        // First 15 columns selected -> 7 characters (9 columns) + 6 spaces
        expectedText += L"AB\u304bC\u304dDE      ";

        VERIFY_ARE_EQUAL(expectedText, text);
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
