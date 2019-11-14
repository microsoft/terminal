// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;

namespace TerminalCoreUnitTests
{
    class InputTest
    {
        TEST_CLASS(InputTest);
        TEST_CLASS_SETUP(ClassSetup)
        {
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);
            auto inputFn = std::bind(&InputTest::_VerifyExpectedInput, this, std::placeholders::_1);
            term.SetWriteInputCallback(inputFn);
            return true;
        };

        TEST_METHOD(AltShiftKey);
        TEST_METHOD(AltSpace);

        void _VerifyExpectedInput(std::wstring& actualInput)
        {
            VERIFY_ARE_EQUAL(expectedinput.size(), actualInput.size());
            VERIFY_ARE_EQUAL(expectedinput, actualInput);
        };

        Terminal term{};
        std::wstring expectedinput{};
    };

    void InputTest::AltShiftKey()
    {
        // Tests GH:637

        // Verify that Alt+a generates a lowercase a on the input
        expectedinput = L"\x1b"
                        "a";
        VERIFY_IS_TRUE(term.SendKeyEvent(L'A', 0, ControlKeyStates::LeftAltPressed));

        // Verify that Alt+shift+a generates a uppercase a on the input
        expectedinput = L"\x1b"
                        "A";
        VERIFY_IS_TRUE(term.SendKeyEvent(L'A', 0, ControlKeyStates::LeftAltPressed | ControlKeyStates::ShiftPressed));
    }

    void InputTest::AltSpace()
    {
        // Make sure we don't handle Alt+Space. The system will use this to
        // bring up the system menu for restore, min/maximimize, size, move,
        // close
        VERIFY_IS_FALSE(term.SendKeyEvent(L' ', 0, ControlKeyStates::LeftAltPressed));
    }
}
