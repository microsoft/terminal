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

        Terminal term{ Terminal::TestDummyMarker{} };
    };

    void InputTest::AltShiftKey()
    {
        // Tests GH:637

        // Verify that Alt+a generates a lowercase a on the input
        VERIFY_ARE_EQUAL(escChar(L'a'), term.SendCharEvent(L'a', 0, ControlKeyStates::LeftAltPressed));

        // Verify that Alt+shift+a generates a uppercase a on the input
        VERIFY_ARE_EQUAL(escChar(L'A'), term.SendCharEvent(L'A', 0, ControlKeyStates::LeftAltPressed | ControlKeyStates::ShiftPressed));
    }

    void InputTest::InvalidKeyEvent()
    {
        // Certain applications like AutoHotKey and its keyboard remapping feature,
        // send us key events using SendInput() whose values are outside of the valid range.
        VERIFY_ARE_EQUAL(unhandled(), term.SendKeyEvent(0, 123, {}, true));
        VERIFY_ARE_EQUAL(unhandled(), term.SendKeyEvent(255, 123, {}, true));
    }
}
