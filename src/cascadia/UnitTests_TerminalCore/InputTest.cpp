// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;

constexpr Microsoft::Console::VirtualTerminal::TerminalInput::OutputType unhandled()
{
    return {};
}

constexpr Microsoft::Console::VirtualTerminal::TerminalInput::OutputType escChar(const wchar_t wch)
{
    const wchar_t buffer[2]{ L'\x1b', wch };
    return { { &buffer[0], 2 } };
}

namespace TerminalCoreUnitTests
{
    class InputTest
    {
        TEST_CLASS(InputTest);

        TEST_METHOD(AltShiftKey);
        TEST_METHOD(InvalidKeyEvent);
        TEST_METHOD(Win32KeyEventsRetainEnhancedKey);

        Terminal term{ Terminal::TestDummyMarker{} };
    };

    void InputTest::AltShiftKey()
    {
        // Tests GH:637

        // Verify that Alt+a generates a lowercase 'a' on the input
        VERIFY_ARE_EQUAL(escChar(L'a'), term.SendCharEvent(L'a', 0, ControlKeyStates::LeftAltPressed));

        // Verify that Alt+shift+a generates an uppercase 'a' on the input
        VERIFY_ARE_EQUAL(escChar(L'A'), term.SendCharEvent(L'A', 0, ControlKeyStates::LeftAltPressed | ControlKeyStates::ShiftPressed));
    }

    void InputTest::InvalidKeyEvent()
    {
        // Certain applications like AutoHotKey and its keyboard remapping feature,
        // send us key events using SendInput() whose values are outside of the valid range.
        VERIFY_ARE_EQUAL(unhandled(), term.SendKeyEvent(0, 123, {}, true));
        VERIFY_ARE_EQUAL(unhandled(), term.SendKeyEvent(255, 123, {}, true));
    }

    void InputTest::Win32KeyEventsRetainEnhancedKey()
    {
        // Tests GH#18120
        // In win32-input-mode, both the press and the release of an extended
        // key (e.g. RightAlt) must encode the ENHANCED_KEY flag (0x100) in the
        // Cs parameter, so that the client can distinguish RightAlt from LeftAlt.
        auto& input = term._getTerminalInput();
        input.SetInputMode(Microsoft::Console::VirtualTerminal::TerminalInput::Mode::Win32, true);
        const auto restore = wil::scope_exit([&]() {
            input.SetInputMode(Microsoft::Console::VirtualTerminal::TerminalInput::Mode::Win32, false);
        });

        // RightAlt down: VK_MENU (18), scanCode 0x38 (56),
        // Cs = RIGHT_ALT_PRESSED | ENHANCED_KEY = 0x0001 | 0x0100 = 257
        const auto down = term.SendKeyEvent(VK_MENU, 0x38, ControlKeyStates::RightAltPressed | ControlKeyStates::EnhancedKey, true);
        VERIFY_IS_TRUE(down.has_value());
        VERIFY_ARE_EQUAL(L"\x1b[18;56;0;1;257;1_", *down);

        // RightAlt up: the modifier itself is no longer pressed,
        // but the key release must still carry ENHANCED_KEY = 0x0100 = 256.
        const auto up = term.SendKeyEvent(VK_MENU, 0x38, ControlKeyStates::EnhancedKey, false);
        VERIFY_IS_TRUE(up.has_value());
        VERIFY_ARE_EQUAL(L"\x1b[18;56;0;0;256;1_", *up);
    }
}
