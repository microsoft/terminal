// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"

#include "stateMachine.hpp"
#include "OutputStateMachineEngine.hpp"

#include "ascii.hpp"

using namespace Microsoft::Console::VirtualTerminal;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class OutputEngineTest;
        }
    }
}

// From VT100.net...
// 9999-10000 is the classic boundary for most parsers parameter values.
// 16383-16384 is the boundary for DECSR commands according to EK-VT520-RM section 4.3.3.2
// 32767-32768 is our boundary SHORT_MAX for the Windows console
#define PARAM_VALUES L"{0, 1, 2, 1000, 9999, 10000, 16383, 16384, 32767, 32768, 50000, 999999999}"

class DummyDispatch final : public TermDispatch
{
public:
    virtual void Execute(const wchar_t /*wchControl*/) override
    {
    }

    virtual void Print(const wchar_t /*wchPrintable*/) override
    {
    }

    virtual void PrintString(const std::wstring_view /*string*/) override
    {
    }
};

class Microsoft::Console::VirtualTerminal::OutputEngineTest final
{
    TEST_CLASS(OutputEngineTest);

    TEST_METHOD(TestEscapePath)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiTest", L"{0,1,2,3,4,5,6,7,8,9,10,11}") // one value for each type of state test below.
        END_TEST_METHOD_PROPERTIES()

        size_t uiTest;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiTest", uiTest));
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        // The OscString state shouldn't escape out after an ESC.
        bool shouldEscapeOut = true;

        switch (uiTest)
        {
        case 0:
        {
            Log::Comment(L"Escape from Ground.");
            mach._state = StateMachine::VTStates::Ground;
            break;
        }
        case 1:
        {
            Log::Comment(L"Escape from Escape.");
            mach._state = StateMachine::VTStates::Escape;
            break;
        }
        case 2:
        {
            Log::Comment(L"Escape from Escape Intermediate");
            mach._state = StateMachine::VTStates::EscapeIntermediate;
            break;
        }
        case 3:
        {
            Log::Comment(L"Escape from CsiEntry");
            mach._state = StateMachine::VTStates::CsiEntry;
            break;
        }
        case 4:
        {
            Log::Comment(L"Escape from CsiIgnore");
            mach._state = StateMachine::VTStates::CsiIgnore;
            break;
        }
        case 5:
        {
            Log::Comment(L"Escape from CsiParam");
            mach._state = StateMachine::VTStates::CsiParam;
            break;
        }
        case 6:
        {
            Log::Comment(L"Escape from CsiIntermediate");
            mach._state = StateMachine::VTStates::CsiIntermediate;
            break;
        }
        case 7:
        {
            Log::Comment(L"Escape from OscParam");
            mach._state = StateMachine::VTStates::OscParam;
            break;
        }
        case 8:
        {
            Log::Comment(L"Escape from OscString");
            shouldEscapeOut = false;
            mach._state = StateMachine::VTStates::OscString;
            break;
        }
        case 9:
        {
            Log::Comment(L"Escape from OscTermination");
            mach._state = StateMachine::VTStates::OscTermination;
            break;
        }
        case 10:
        {
            Log::Comment(L"Escape from Ss3Entry");
            mach._state = StateMachine::VTStates::Ss3Entry;
            break;
        }
        case 11:
        {
            Log::Comment(L"Escape from Ss3Param");
            mach._state = StateMachine::VTStates::Ss3Param;
            break;
        }
        }

        mach.ProcessCharacter(AsciiChars::ESC);
        if (shouldEscapeOut)
        {
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        }
    }

    TEST_METHOD(TestEscapeImmediatePath)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'#');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::EscapeIntermediate);
        mach.ProcessCharacter(L'(');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::EscapeIntermediate);
        mach.ProcessCharacter(L')');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::EscapeIntermediate);
        mach.ProcessCharacter(L'#');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::EscapeIntermediate);
        mach.ProcessCharacter(L'6');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestEscapeThenC0Path)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        // When we see a C0 control char in the escape state, the Output engine
        // should execute it, without interrupting the sequence it's currently
        // processing
        mach.ProcessCharacter(L'\x03');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'3');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'1');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestGroundPrint)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(L'a');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiEntry)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestC1CsiEntry)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(L'\x9b');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiImmediate)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'$');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIntermediate);
        mach.ProcessCharacter(L'#');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIntermediate);
        mach.ProcessCharacter(L'%');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIntermediate);
        mach.ProcessCharacter(L'v');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiParam)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'3');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'2');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'4');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'8');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'J');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestLeadingZeroCsiParam)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        for (int i = 0; i < 50; i++) // Any number of leading zeros should be supported
        {
            mach.ProcessCharacter(L'0');
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        }
        for (int i = 0; i < 5; i++) // We're only expecting to be able to keep 5 digits max
        {
            mach.ProcessCharacter((wchar_t)(L'1' + i));
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        }
        VERIFY_ARE_EQUAL(mach._parameters.back(), 12345u);
        mach.ProcessCharacter(L'J');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiIgnore)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L':');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'3');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'q');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'4');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L':');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'8');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'J');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'[');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'4');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiParam);
        mach.ProcessCharacter(L'#');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIntermediate);
        mach.ProcessCharacter(L':');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'8');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiIgnore);
        mach.ProcessCharacter(L'J');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestOscStringSimple)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L'0');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'o');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'e');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L' ');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L't');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'e');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'x');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L't');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L'0');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'o');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'e');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L' ');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L't');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'e');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L'x');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L't');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscTermination);
        mach.ProcessCharacter(L'\\');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }
    TEST_METHOD(TestLongOscString)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L'0');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L';');
        for (int i = 0; i < 260u; i++) // The buffer is only 256 long, so any longer value should work :P
        {
            mach.ProcessCharacter(L's');
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        }
        VERIFY_ARE_EQUAL(mach._oscString.size(), 260u);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(NormalTestOscParam)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        for (int i = 0; i < 5; i++) // We're only expecting to be able to keep 5 digits max
        {
            mach.ProcessCharacter((wchar_t)(L'1' + i));
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._oscParameter, 12345u);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestLeadingZeroOscParam)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        for (int i = 0; i < 50; i++) // Any number of leading zeros should be supported
        {
            mach.ProcessCharacter(L'0');
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        for (int i = 0; i < 5; i++) // We're only expecting to be able to keep 5 digits max
        {
            mach.ProcessCharacter((wchar_t)(L'1' + i));
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._oscParameter, 12345u);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestLongOscParam)
    {
        auto dispatch = std::make_unique<DummyDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        constexpr auto sizeMax = std::numeric_limits<size_t>::max();
        const auto sizeMaxStr = wil::str_printf<std::wstring>(L"%zu", sizeMax);
        for (auto& wch : sizeMaxStr)
        {
            mach.ProcessCharacter(wch);
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._oscParameter, MAX_PARAMETER_VALUE);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        for (const auto& wch : sizeMaxStr)
        {
            mach.ProcessCharacter(wch);
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._oscParameter, MAX_PARAMETER_VALUE);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }
};

class StatefulDispatch final : public TermDispatch
{
public:
    virtual void Execute(const wchar_t /*wchControl*/) override
    {
    }

    virtual void Print(const wchar_t /*wchPrintable*/) override
    {
    }

    virtual void PrintString(const std::wstring_view /*string*/) override
    {
    }

    StatefulDispatch() :
        _cursorDistance{ 0 },
        _line{ 0 },
        _column{ 0 },
        _cursorUp{ false },
        _cursorDown{ false },
        _cursorBackward{ false },
        _cursorForward{ false },
        _cursorNextLine{ false },
        _cursorPreviousLine{ false },
        _cursorHorizontalPositionAbsolute{ false },
        _verticalLinePositionAbsolute{ false },
        _horizontalPositionRelative{ false },
        _verticalPositionRelative{ false },
        _cursorPosition{ false },
        _cursorSave{ false },
        _cursorLoad{ false },
        _cursorVisible{ true },
        _eraseDisplay{ false },
        _eraseLine{ false },
        _insertCharacter{ false },
        _deleteCharacter{ false },
        _eraseType{ (DispatchTypes::EraseType)-1 },
        _setGraphics{ false },
        _statusReportType{ (DispatchTypes::AnsiStatusType)-1 },
        _deviceStatusReport{ false },
        _deviceAttributes{ false },
        _vt52DeviceAttributes{ false },
        _isAltBuffer{ false },
        _cursorKeysMode{ false },
        _cursorBlinking{ true },
        _isInAnsiMode{ true },
        _isScreenModeReversed{ false },
        _isOriginModeRelative{ false },
        _isAutoWrapEnabled{ true },
        _warningBell{ false },
        _carriageReturn{ false },
        _lineFeed{ false },
        _lineFeedType{ (DispatchTypes::LineFeedType)-1 },
        _reverseLineFeed{ false },
        _forwardTab{ false },
        _numTabs{ 0 },
        _isDECCOLMAllowed{ false },
        _windowWidth{ 80 },
        _win32InputMode{ false },
        _options{ s_cMaxOptions, static_cast<DispatchTypes::GraphicsOptions>(s_uiGraphicsCleared) } // fill with cleared option
    {
    }

    void ClearState()
    {
        StatefulDispatch dispatch;
        *this = dispatch;
    }

    bool CursorUp(_In_ size_t const uiDistance) noexcept override
    {
        _cursorUp = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorDown(_In_ size_t const uiDistance) noexcept override
    {
        _cursorDown = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorBackward(_In_ size_t const uiDistance) noexcept override
    {
        _cursorBackward = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorForward(_In_ size_t const uiDistance) noexcept override
    {
        _cursorForward = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorNextLine(_In_ size_t const uiDistance) noexcept override
    {
        _cursorNextLine = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorPrevLine(_In_ size_t const uiDistance) noexcept override
    {
        _cursorPreviousLine = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorHorizontalPositionAbsolute(_In_ size_t const uiPosition) noexcept override
    {
        _cursorHorizontalPositionAbsolute = true;
        _cursorDistance = uiPosition;
        return true;
    }

    bool VerticalLinePositionAbsolute(_In_ size_t const uiPosition) noexcept override
    {
        _verticalLinePositionAbsolute = true;
        _cursorDistance = uiPosition;
        return true;
    }

    bool HorizontalPositionRelative(_In_ size_t const uiDistance) noexcept override
    {
        _horizontalPositionRelative = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool VerticalPositionRelative(_In_ size_t const uiDistance) noexcept override
    {
        _verticalPositionRelative = true;
        _cursorDistance = uiDistance;
        return true;
    }

    bool CursorPosition(_In_ size_t const uiLine, _In_ size_t const uiColumn) noexcept override
    {
        _cursorPosition = true;
        _line = uiLine;
        _column = uiColumn;
        return true;
    }

    bool CursorSaveState() noexcept override
    {
        _cursorSave = true;
        return true;
    }

    bool CursorRestoreState() noexcept override
    {
        _cursorLoad = true;
        return true;
    }

    bool EraseInDisplay(const DispatchTypes::EraseType eraseType) noexcept override
    {
        _eraseDisplay = true;
        _eraseType = eraseType;
        return true;
    }

    bool EraseInLine(const DispatchTypes::EraseType eraseType) noexcept override
    {
        _eraseLine = true;
        _eraseType = eraseType;
        return true;
    }

    bool InsertCharacter(_In_ size_t const uiCount) noexcept override
    {
        _insertCharacter = true;
        _cursorDistance = uiCount;
        return true;
    }

    bool DeleteCharacter(_In_ size_t const uiCount) noexcept override
    {
        _deleteCharacter = true;
        _cursorDistance = uiCount;
        return true;
    }

    bool CursorVisibility(const bool fIsVisible) noexcept override
    {
        _cursorVisible = fIsVisible;
        return true;
    }

    bool SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> options) noexcept override
    try
    {
        _options.assign(options.cbegin(), options.cend());
        _setGraphics = true;
        return true;
    }
    CATCH_LOG_RETURN_FALSE()

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType) noexcept override
    {
        _deviceStatusReport = true;
        _statusReportType = statusType;

        return true;
    }

    bool DeviceAttributes() noexcept override
    {
        _deviceAttributes = true;

        return true;
    }

    bool Vt52DeviceAttributes() noexcept override
    {
        _vt52DeviceAttributes = true;

        return true;
    }

    bool _PrivateModeParamsHelper(_In_ DispatchTypes::PrivateModeParams const param, const bool fEnable)
    {
        bool fSuccess = false;
        switch (param)
        {
        case DispatchTypes::PrivateModeParams::DECCKM_CursorKeysMode:
            // set - Enable Application Mode, reset - Numeric/normal mode
            fSuccess = SetVirtualTerminalInputMode(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::DECANM_AnsiMode:
            fSuccess = SetAnsiMode(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::DECCOLM_SetNumberOfColumns:
            fSuccess = SetColumns(static_cast<size_t>(fEnable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns));
            break;
        case DispatchTypes::PrivateModeParams::DECSCNM_ScreenMode:
            fSuccess = SetScreenMode(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::DECOM_OriginMode:
            // The cursor is also moved to the new home position when the origin mode is set or reset.
            fSuccess = SetOriginMode(fEnable) && CursorPosition(1, 1);
            break;
        case DispatchTypes::PrivateModeParams::DECAWM_AutoWrapMode:
            fSuccess = SetAutoWrapMode(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::ATT610_StartCursorBlink:
            fSuccess = EnableCursorBlinking(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::DECTCEM_TextCursorEnableMode:
            fSuccess = CursorVisibility(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::XTERM_EnableDECCOLMSupport:
            fSuccess = EnableDECCOLMSupport(fEnable);
            break;
        case DispatchTypes::PrivateModeParams::ASB_AlternateScreenBuffer:
            fSuccess = fEnable ? UseAlternateScreenBuffer() : UseMainScreenBuffer();
            break;
        case DispatchTypes::PrivateModeParams::W32IM_Win32InputMode:
            fSuccess = EnableWin32InputMode(fEnable);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
        return fSuccess;
    }

    bool _SetResetPrivateModesHelper(const std::basic_string_view<DispatchTypes::PrivateModeParams> params,
                                     const bool enable)
    {
        size_t cFailures = 0;
        for (const auto& p : params)
        {
            cFailures += _PrivateModeParamsHelper(p, enable) ? 0 : 1; // increment the number of failures if we fail.
        }
        return cFailures == 0;
    }

    bool SetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params) noexcept override
    {
        return _SetResetPrivateModesHelper(params, true);
    }

    bool ResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params) noexcept override
    {
        return _SetResetPrivateModesHelper(params, false);
    }

    bool SetColumns(_In_ size_t const uiColumns) noexcept override
    {
        _windowWidth = uiColumns;
        return true;
    }

    bool SetVirtualTerminalInputMode(const bool fApplicationMode) noexcept
    {
        _cursorKeysMode = fApplicationMode;
        return true;
    }

    bool EnableWin32InputMode(const bool enable) noexcept
    {
        _win32InputMode = enable;
        return true;
    }

    bool EnableCursorBlinking(const bool bEnable) noexcept override
    {
        _cursorBlinking = bEnable;
        return true;
    }

    bool SetAnsiMode(const bool ansiMode) noexcept override
    {
        _isInAnsiMode = ansiMode;
        return true;
    }

    bool SetScreenMode(const bool reverseMode) noexcept override
    {
        _isScreenModeReversed = reverseMode;
        return true;
    }

    bool SetOriginMode(const bool fRelativeMode) noexcept override
    {
        _isOriginModeRelative = fRelativeMode;
        return true;
    }

    bool SetAutoWrapMode(const bool wrapAtEOL) noexcept override
    {
        _isAutoWrapEnabled = wrapAtEOL;
        return true;
    }

    bool WarningBell() noexcept override
    {
        _warningBell = true;
        return true;
    }

    bool CarriageReturn() noexcept override
    {
        _carriageReturn = true;
        return true;
    }

    bool LineFeed(const DispatchTypes::LineFeedType lineFeedType) noexcept override
    {
        _lineFeed = true;
        _lineFeedType = lineFeedType;
        return true;
    }

    bool ReverseLineFeed() noexcept override
    {
        _reverseLineFeed = true;
        return true;
    }

    bool ForwardTab(const size_t numTabs) noexcept override
    {
        _forwardTab = true;
        _numTabs = numTabs;
        return true;
    }

    bool EnableDECCOLMSupport(const bool fEnabled) noexcept override
    {
        _isDECCOLMAllowed = fEnabled;
        return true;
    }

    bool UseAlternateScreenBuffer() noexcept override
    {
        _isAltBuffer = true;
        return true;
    }

    bool UseMainScreenBuffer() noexcept override
    {
        _isAltBuffer = false;
        return true;
    }

    size_t _cursorDistance;
    size_t _line;
    size_t _column;
    bool _cursorUp;
    bool _cursorDown;
    bool _cursorBackward;
    bool _cursorForward;
    bool _cursorNextLine;
    bool _cursorPreviousLine;
    bool _cursorHorizontalPositionAbsolute;
    bool _verticalLinePositionAbsolute;
    bool _horizontalPositionRelative;
    bool _verticalPositionRelative;
    bool _cursorPosition;
    bool _cursorSave;
    bool _cursorLoad;
    bool _cursorVisible;
    bool _eraseDisplay;
    bool _eraseLine;
    bool _insertCharacter;
    bool _deleteCharacter;
    DispatchTypes::EraseType _eraseType;
    bool _setGraphics;
    DispatchTypes::AnsiStatusType _statusReportType;
    bool _deviceStatusReport;
    bool _deviceAttributes;
    bool _vt52DeviceAttributes;
    bool _isAltBuffer;
    bool _cursorKeysMode;
    bool _cursorBlinking;
    bool _isInAnsiMode;
    bool _isScreenModeReversed;
    bool _isOriginModeRelative;
    bool _isAutoWrapEnabled;
    bool _warningBell;
    bool _carriageReturn;
    bool _lineFeed;
    DispatchTypes::LineFeedType _lineFeedType;
    bool _reverseLineFeed;
    bool _forwardTab;
    size_t _numTabs;
    bool _isDECCOLMAllowed;
    size_t _windowWidth;
    bool _win32InputMode;

    static const size_t s_cMaxOptions = 16;
    static const size_t s_uiGraphicsCleared = UINT_MAX;
    std::vector<DispatchTypes::GraphicsOptions> _options;
};

class StateMachineExternalTest final
{
    TEST_CLASS(StateMachineExternalTest);

    TEST_METHOD_SETUP(SetupState)
    {
        return true;
    }

    void InsertNumberToMachine(StateMachine* const pMachine, size_t number)
    {
        static const size_t cchBufferMax = 20;

        wchar_t pwszDistance[cchBufferMax];
        int cchDistance = swprintf_s(pwszDistance, cchBufferMax, L"%zu", number);

        if (cchDistance > 0 && cchDistance < cchBufferMax)
        {
            for (int i = 0; i < cchDistance; i++)
            {
                pMachine->ProcessCharacter(pwszDistance[i]);
            }
        }
    }

    void ApplyParameterBoundary(size_t* uiExpected, size_t uiGiven)
    {
        // 0 and 1 should be 1. Use the preset value.
        // 1-MAX_PARAMETER_VALUE should be what we set.
        // > MAX_PARAMETER_VALUE should be MAX_PARAMETER_VALUE.
        if (uiGiven <= 1)
        {
            *uiExpected = 1u;
        }
        else if (uiGiven > 1 && uiGiven <= MAX_PARAMETER_VALUE)
        {
            *uiExpected = uiGiven;
        }
        else if (uiGiven > MAX_PARAMETER_VALUE)
        {
            *uiExpected = MAX_PARAMETER_VALUE; // 32767 is our max value.
        }
    }

    void TestCsiCursorMovement(wchar_t const wchCommand,
                               size_t const uiDistance,
                               const bool fUseDistance,
                               const bool* const pfFlag,
                               StateMachine& mach,
                               StatefulDispatch& dispatch)
    {
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        if (fUseDistance)
        {
            InsertNumberToMachine(&mach, uiDistance);
        }

        mach.ProcessCharacter(wchCommand);

        VERIFY_IS_TRUE(*pfFlag);

        size_t uiExpectedDistance = 1u;

        if (fUseDistance)
        {
            ApplyParameterBoundary(&uiExpectedDistance, uiDistance);
        }

        VERIFY_ARE_EQUAL(dispatch._cursorDistance, uiExpectedDistance);
    }

    TEST_METHOD(TestCsiCursorMovementWithValues)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDistance", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        size_t uiDistance;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDistance", uiDistance));

        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        TestCsiCursorMovement(L'A', uiDistance, true, &pDispatch->_cursorUp, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'B', uiDistance, true, &pDispatch->_cursorDown, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'C', uiDistance, true, &pDispatch->_cursorForward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'D', uiDistance, true, &pDispatch->_cursorBackward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'E', uiDistance, true, &pDispatch->_cursorNextLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'F', uiDistance, true, &pDispatch->_cursorPreviousLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'G', uiDistance, true, &pDispatch->_cursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'`', uiDistance, true, &pDispatch->_cursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'd', uiDistance, true, &pDispatch->_verticalLinePositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'a', uiDistance, true, &pDispatch->_horizontalPositionRelative, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'e', uiDistance, true, &pDispatch->_verticalPositionRelative, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'@', uiDistance, true, &pDispatch->_insertCharacter, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'P', uiDistance, true, &pDispatch->_deleteCharacter, mach, *pDispatch);
    }

    TEST_METHOD(TestCsiCursorMovementWithoutValues)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        size_t uiDistance = 9999; // this value should be ignored with the false below.
        TestCsiCursorMovement(L'A', uiDistance, false, &pDispatch->_cursorUp, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'B', uiDistance, false, &pDispatch->_cursorDown, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'C', uiDistance, false, &pDispatch->_cursorForward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'D', uiDistance, false, &pDispatch->_cursorBackward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'E', uiDistance, false, &pDispatch->_cursorNextLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'F', uiDistance, false, &pDispatch->_cursorPreviousLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'G', uiDistance, false, &pDispatch->_cursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'`', uiDistance, false, &pDispatch->_cursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'd', uiDistance, false, &pDispatch->_verticalLinePositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'a', uiDistance, false, &pDispatch->_horizontalPositionRelative, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'e', uiDistance, false, &pDispatch->_verticalPositionRelative, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'@', uiDistance, false, &pDispatch->_insertCharacter, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'P', uiDistance, false, &pDispatch->_deleteCharacter, mach, *pDispatch);
    }

    TEST_METHOD(TestCsiCursorPosition)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiRow", PARAM_VALUES)
            TEST_METHOD_PROPERTY(L"Data:uiCol", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        size_t uiRow;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiRow", uiRow));
        size_t uiCol;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiCol", uiCol));

        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        InsertNumberToMachine(&mach, uiRow);
        mach.ProcessCharacter(L';');
        InsertNumberToMachine(&mach, uiCol);
        mach.ProcessCharacter(L'H');

        // bound the row/col values by the max we expect
        ApplyParameterBoundary(&uiRow, uiRow);
        ApplyParameterBoundary(&uiCol, uiCol);

        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_line, uiRow);
        VERIFY_ARE_EQUAL(pDispatch->_column, uiCol);
    }

    TEST_METHOD(TestCsiCursorPositionWithOnlyRow)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiRow", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        size_t uiRow;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiRow", uiRow));

        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        InsertNumberToMachine(&mach, uiRow);
        mach.ProcessCharacter(L'H');

        // bound the row/col values by the max we expect
        ApplyParameterBoundary(&uiRow, uiRow);

        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_line, uiRow);
        VERIFY_ARE_EQUAL(pDispatch->_column, (size_t)1); // Without the second param, the column should always be the default
    }

    TEST_METHOD(TestCursorSaveLoad)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'7');
        VERIFY_IS_TRUE(pDispatch->_cursorSave);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'8');
        VERIFY_IS_TRUE(pDispatch->_cursorLoad);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L's');
        VERIFY_IS_TRUE(pDispatch->_cursorSave);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'u');
        VERIFY_IS_TRUE(pDispatch->_cursorLoad);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorKeysMode)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?1h");
        VERIFY_IS_TRUE(pDispatch->_cursorKeysMode);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1l");
        VERIFY_IS_FALSE(pDispatch->_cursorKeysMode);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestAnsiMode)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?2l");
        VERIFY_IS_FALSE(pDispatch->_isInAnsiMode);

        pDispatch->ClearState();
        pDispatch->_isInAnsiMode = false;
        mach.SetAnsiMode(false);

        mach.ProcessString(L"\x1b<");
        VERIFY_IS_TRUE(pDispatch->_isInAnsiMode);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestSetNumberOfColumns)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?3h");
        VERIFY_ARE_EQUAL(pDispatch->_windowWidth, static_cast<size_t>(DispatchTypes::s_sDECCOLMSetColumns));

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?3l");
        VERIFY_ARE_EQUAL(pDispatch->_windowWidth, static_cast<size_t>(DispatchTypes::s_sDECCOLMResetColumns));

        pDispatch->ClearState();
    }

    TEST_METHOD(TestScreenMode)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?5h");
        VERIFY_IS_TRUE(pDispatch->_isScreenModeReversed);

        pDispatch->ClearState();
        pDispatch->_isScreenModeReversed = true;

        mach.ProcessString(L"\x1b[?5l");
        VERIFY_IS_FALSE(pDispatch->_isScreenModeReversed);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestOriginMode)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?6h");
        VERIFY_IS_TRUE(pDispatch->_isOriginModeRelative);
        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_line, 1u);
        VERIFY_ARE_EQUAL(pDispatch->_column, 1u);

        pDispatch->ClearState();
        pDispatch->_isOriginModeRelative = true;

        mach.ProcessString(L"\x1b[?6l");
        VERIFY_IS_FALSE(pDispatch->_isOriginModeRelative);
        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_line, 1u);
        VERIFY_ARE_EQUAL(pDispatch->_column, 1u);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestAutoWrapMode)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?7l");
        VERIFY_IS_FALSE(pDispatch->_isAutoWrapEnabled);

        pDispatch->ClearState();
        pDispatch->_isAutoWrapEnabled = false;

        mach.ProcessString(L"\x1b[?7h");
        VERIFY_IS_TRUE(pDispatch->_isAutoWrapEnabled);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorBlinking)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?12h");
        VERIFY_IS_TRUE(pDispatch->_cursorBlinking);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?12l");
        VERIFY_IS_FALSE(pDispatch->_cursorBlinking);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorVisibility)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?25h");
        VERIFY_IS_TRUE(pDispatch->_cursorVisible);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?25l");
        VERIFY_IS_FALSE(pDispatch->_cursorVisible);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestAltBufferSwapping)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?1049h");
        VERIFY_IS_TRUE(pDispatch->_isAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049h");
        VERIFY_IS_TRUE(pDispatch->_isAltBuffer);
        mach.ProcessString(L"\x1b[?1049h");
        VERIFY_IS_TRUE(pDispatch->_isAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049l");
        VERIFY_IS_FALSE(pDispatch->_isAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049h");
        VERIFY_IS_TRUE(pDispatch->_isAltBuffer);
        mach.ProcessString(L"\x1b[?1049l");
        VERIFY_IS_FALSE(pDispatch->_isAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049l");
        VERIFY_IS_FALSE(pDispatch->_isAltBuffer);
        mach.ProcessString(L"\x1b[?1049l");
        VERIFY_IS_FALSE(pDispatch->_isAltBuffer);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestEnableDECCOLMSupport)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        mach.ProcessString(L"\x1b[?40h");
        VERIFY_IS_TRUE(pDispatch->_isDECCOLMAllowed);

        pDispatch->ClearState();
        pDispatch->_isDECCOLMAllowed = true;

        mach.ProcessString(L"\x1b[?40l");
        VERIFY_IS_FALSE(pDispatch->_isDECCOLMAllowed);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestErase)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiEraseOperation", L"{0, 1}") // for "display" and "line" type erase operations
            TEST_METHOD_PROPERTY(L"Data:uiDispatchTypes::EraseType", L"{0, 1, 2, 10}") // maps to DispatchTypes::EraseType enum class options.
        END_TEST_METHOD_PROPERTIES()

        size_t uiEraseOperation;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiEraseOperation", uiEraseOperation));
        size_t uiDispatchTypes;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDispatchTypes::EraseType", uiDispatchTypes));

        WCHAR wchOp = L'\0';
        bool* pfOperationCallback = nullptr;

        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        switch (uiEraseOperation)
        {
        case 0:
            wchOp = L'J';
            pfOperationCallback = &pDispatch->_eraseDisplay;
            break;
        case 1:
            wchOp = L'K';
            pfOperationCallback = &pDispatch->_eraseLine;
            break;
        default:
            VERIFY_FAIL(L"Unknown erase operation permutation.");
        }

        VERIFY_IS_NOT_NULL(wchOp);
        VERIFY_IS_NOT_NULL(pfOperationCallback);

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        DispatchTypes::EraseType expectedDispatchTypes;

        switch (uiDispatchTypes)
        {
        case 0:
            expectedDispatchTypes = DispatchTypes::EraseType::ToEnd;
            InsertNumberToMachine(&mach, uiDispatchTypes);
            break;
        case 1:
            expectedDispatchTypes = DispatchTypes::EraseType::FromBeginning;
            InsertNumberToMachine(&mach, uiDispatchTypes);
            break;
        case 2:
            expectedDispatchTypes = DispatchTypes::EraseType::All;
            InsertNumberToMachine(&mach, uiDispatchTypes);
            break;
        case 10:
            // Do nothing. Default case of 10 should be like a 0 to the end.
            expectedDispatchTypes = DispatchTypes::EraseType::ToEnd;
            break;
        }

        mach.ProcessCharacter(wchOp);

        VERIFY_IS_TRUE(*pfOperationCallback);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);
    }

    void VerifyDispatchTypes(const std::basic_string_view<DispatchTypes::GraphicsOptions> expectedOptions,
                             const StatefulDispatch& dispatch)
    {
        VERIFY_ARE_EQUAL(expectedOptions.size(), dispatch._options.size());
        bool optionsValid = true;

        for (size_t i = 0; i < dispatch._options.size(); i++)
        {
            auto expectedOption = (DispatchTypes::GraphicsOptions)dispatch.s_uiGraphicsCleared;

            if (i < expectedOptions.size())
            {
                expectedOption = expectedOptions.at(i);
            }

            optionsValid = expectedOption == dispatch._options.at(i);

            if (!optionsValid)
            {
                Log::Comment(NoThrowString().Format(L"Graphics option match failed, index [%zu]. Expected: '%d' Actual: '%d'", i, expectedOption, dispatch._options.at(i)));
                break;
            }
        }

        VERIFY_IS_TRUE(optionsValid);
    }

    TEST_METHOD(TestSetGraphicsRendition)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        DispatchTypes::GraphicsOptions rgExpected[17];

        Log::Comment(L"Test 1: Check default case.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes({ rgExpected, 1 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check clear/0 case.");

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'0');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes({ rgExpected, 1 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 3: Check 'handful of options' case.");

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'7');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'3');
        mach.ProcessCharacter(L'0');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L'5');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[2] = DispatchTypes::GraphicsOptions::Negative;
        rgExpected[3] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        rgExpected[4] = DispatchTypes::GraphicsOptions::BackgroundMagenta;
        VerifyDispatchTypes({ rgExpected, 5 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 4: Check 'many options' (>16) case.");

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'4');
        mach.ProcessCharacter(L';');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[2] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[3] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[4] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[5] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[6] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[7] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[8] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[9] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[10] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[11] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[12] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[13] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[14] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[15] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[16] = DispatchTypes::GraphicsOptions::BoldBright;
        VerifyDispatchTypes({ rgExpected, 17 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.a: Test an empty param at the end of a sequence");

        std::wstring sequence = L"\x1b[1;m";
        mach.ProcessString(sequence);
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes({ rgExpected, 2 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.b: Test an empty param in the middle of a sequence");

        sequence = L"\x1b[1;;1m";
        mach.ProcessString(sequence);
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Off;
        rgExpected[2] = DispatchTypes::GraphicsOptions::BoldBright;
        VerifyDispatchTypes({ rgExpected, 3 }, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.c: Test an empty param at the start of a sequence");

        sequence = L"\x1b[;31;1m";
        mach.ProcessString(sequence);
        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundRed;
        rgExpected[2] = DispatchTypes::GraphicsOptions::BoldBright;
        VerifyDispatchTypes({ rgExpected, 3 }, *pDispatch);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestDeviceStatusReport)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        Log::Comment(L"Test 1: Check empty case. Should fail.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_FALSE(pDispatch->_deviceStatusReport);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check OS (operating status) case 5. Should succeed.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'5');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_TRUE(pDispatch->_deviceStatusReport);
        VERIFY_ARE_EQUAL(DispatchTypes::AnsiStatusType::OS_OperatingStatus, pDispatch->_statusReportType);

        pDispatch->ClearState();

        Log::Comment(L"Test 3: Check CPR (cursor position report) case 6. Should succeed.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'6');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_TRUE(pDispatch->_deviceStatusReport);
        VERIFY_ARE_EQUAL(DispatchTypes::AnsiStatusType::CPR_CursorPositionReport, pDispatch->_statusReportType);

        pDispatch->ClearState();

        Log::Comment(L"Test 4: Check unimplemented case 1. Should fail.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_FALSE(pDispatch->_deviceStatusReport);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestDeviceAttributes)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        Log::Comment(L"Test 1: Check default case, no params.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_TRUE(pDispatch->_deviceAttributes);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check default case, 0 param.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'0');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_TRUE(pDispatch->_deviceAttributes);

        pDispatch->ClearState();

        Log::Comment(L"Test 3: Check fail case, 1 (or any other) param.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_FALSE(pDispatch->_deviceAttributes);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestStrings)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        DispatchTypes::GraphicsOptions rgExpected[16];
        DispatchTypes::EraseType expectedDispatchTypes;
        ///////////////////////////////////////////////////////////////////////

        Log::Comment(L"Test 1: Basic String processing. One sequence in a string.");
        mach.ProcessString(L"\x1b[0m");

        VERIFY_IS_TRUE(pDispatch->_setGraphics);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////

        Log::Comment(L"Test 2: A couple of sequences all in one string");

        mach.ProcessString(L"\x1b[1;4;7;30;45m\x1b[2J");
        VERIFY_IS_TRUE(pDispatch->_setGraphics);
        VERIFY_IS_TRUE(pDispatch->_eraseDisplay);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[2] = DispatchTypes::GraphicsOptions::Negative;
        rgExpected[3] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        rgExpected[4] = DispatchTypes::GraphicsOptions::BackgroundMagenta;
        expectedDispatchTypes = DispatchTypes::EraseType::All;
        VerifyDispatchTypes({ rgExpected, 5 }, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 3: Two sequences separated by a non-sequence of characters");

        mach.ProcessString(L"\x1b[1;30mHello World\x1b[2J");

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_setGraphics);
        VERIFY_IS_TRUE(pDispatch->_eraseDisplay);

        VerifyDispatchTypes({ rgExpected, 2 }, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 4: An entire sequence broke into multiple strings");
        mach.ProcessString(L"\x1b[1;");
        VERIFY_IS_FALSE(pDispatch->_setGraphics);
        VERIFY_IS_FALSE(pDispatch->_eraseDisplay);

        mach.ProcessString(L"30mHello World\x1b[2J");

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_setGraphics);
        VERIFY_IS_TRUE(pDispatch->_eraseDisplay);

        VerifyDispatchTypes({ rgExpected, 2 }, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 5: A sequence with mixed ProcessCharacter and ProcessString calls");

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;

        mach.ProcessString(L"\x1b[1;");
        VERIFY_IS_FALSE(pDispatch->_setGraphics);
        VERIFY_IS_FALSE(pDispatch->_eraseDisplay);

        mach.ProcessCharacter(L'3');
        VERIFY_IS_FALSE(pDispatch->_setGraphics);
        VERIFY_IS_FALSE(pDispatch->_eraseDisplay);

        mach.ProcessCharacter(L'0');
        VERIFY_IS_FALSE(pDispatch->_setGraphics);
        VERIFY_IS_FALSE(pDispatch->_eraseDisplay);

        mach.ProcessCharacter(L'm');

        VERIFY_IS_TRUE(pDispatch->_setGraphics);
        VERIFY_IS_FALSE(pDispatch->_eraseDisplay);
        VerifyDispatchTypes({ rgExpected, 2 }, *pDispatch);

        mach.ProcessString(L"Hello World\x1b[2J");

        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_eraseDisplay);

        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestLineFeed)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        Log::Comment(L"IND (Index) escape sequence");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'D');

        VERIFY_IS_TRUE(pDispatch->_lineFeed);
        VERIFY_ARE_EQUAL(DispatchTypes::LineFeedType::WithoutReturn, pDispatch->_lineFeedType);

        pDispatch->ClearState();

        Log::Comment(L"NEL (Next Line) escape sequence");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'E');

        VERIFY_IS_TRUE(pDispatch->_lineFeed);
        VERIFY_ARE_EQUAL(DispatchTypes::LineFeedType::WithReturn, pDispatch->_lineFeedType);

        pDispatch->ClearState();

        Log::Comment(L"LF (Line Feed) control code");
        mach.ProcessCharacter(AsciiChars::LF);

        VERIFY_IS_TRUE(pDispatch->_lineFeed);
        VERIFY_ARE_EQUAL(DispatchTypes::LineFeedType::DependsOnMode, pDispatch->_lineFeedType);

        pDispatch->ClearState();

        Log::Comment(L"FF (Form Feed) control code");
        mach.ProcessCharacter(AsciiChars::FF);

        VERIFY_IS_TRUE(pDispatch->_lineFeed);
        VERIFY_ARE_EQUAL(DispatchTypes::LineFeedType::DependsOnMode, pDispatch->_lineFeedType);

        pDispatch->ClearState();

        Log::Comment(L"VT (Vertical Tab) control code");
        mach.ProcessCharacter(AsciiChars::VT);

        VERIFY_IS_TRUE(pDispatch->_lineFeed);
        VERIFY_ARE_EQUAL(DispatchTypes::LineFeedType::DependsOnMode, pDispatch->_lineFeedType);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestControlCharacters)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        Log::Comment(L"BEL (Warning Bell) control character");
        mach.ProcessCharacter(AsciiChars::BEL);

        VERIFY_IS_TRUE(pDispatch->_warningBell);

        pDispatch->ClearState();

        Log::Comment(L"BS (Back Space) control character");
        mach.ProcessCharacter(AsciiChars::BS);

        VERIFY_IS_TRUE(pDispatch->_cursorBackward);
        VERIFY_ARE_EQUAL(1u, pDispatch->_cursorDistance);

        pDispatch->ClearState();

        Log::Comment(L"CR (Carriage Return) control character");
        mach.ProcessCharacter(AsciiChars::CR);

        VERIFY_IS_TRUE(pDispatch->_carriageReturn);

        pDispatch->ClearState();

        Log::Comment(L"HT (Horizontal Tab) control character");
        mach.ProcessCharacter(AsciiChars::TAB);

        VERIFY_IS_TRUE(pDispatch->_forwardTab);
        VERIFY_ARE_EQUAL(1u, pDispatch->_numTabs);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestVt52Sequences)
    {
        auto dispatch = std::make_unique<StatefulDispatch>();
        auto pDispatch = dispatch.get();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));
        StateMachine mach(std::move(engine));

        // ANSI mode must be reset for VT52 sequences to be recognized.
        mach.SetAnsiMode(false);

        Log::Comment(L"Cursor Up");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'A');
        VERIFY_IS_TRUE(pDispatch->_cursorUp);
        VERIFY_ARE_EQUAL(1u, pDispatch->_cursorDistance);

        pDispatch->ClearState();

        Log::Comment(L"Cursor Down");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'B');
        VERIFY_IS_TRUE(pDispatch->_cursorDown);
        VERIFY_ARE_EQUAL(1u, pDispatch->_cursorDistance);

        pDispatch->ClearState();

        Log::Comment(L"Cursor Right");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'C');
        VERIFY_IS_TRUE(pDispatch->_cursorForward);
        VERIFY_ARE_EQUAL(1u, pDispatch->_cursorDistance);

        pDispatch->ClearState();

        Log::Comment(L"Cursor Left");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'D');
        VERIFY_IS_TRUE(pDispatch->_cursorBackward);
        VERIFY_ARE_EQUAL(1u, pDispatch->_cursorDistance);

        pDispatch->ClearState();

        Log::Comment(L"Cursor to Home");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'H');
        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(1u, pDispatch->_line);
        VERIFY_ARE_EQUAL(1u, pDispatch->_column);

        pDispatch->ClearState();

        Log::Comment(L"Reverse Line Feed");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'I');
        VERIFY_IS_TRUE(pDispatch->_reverseLineFeed);

        pDispatch->ClearState();

        Log::Comment(L"Erase to End of Screen");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'J');
        VERIFY_IS_TRUE(pDispatch->_eraseDisplay);
        VERIFY_ARE_EQUAL(DispatchTypes::EraseType::ToEnd, pDispatch->_eraseType);

        pDispatch->ClearState();

        Log::Comment(L"Erase to End of Line");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'K');
        VERIFY_IS_TRUE(pDispatch->_eraseLine);
        VERIFY_ARE_EQUAL(DispatchTypes::EraseType::ToEnd, pDispatch->_eraseType);

        pDispatch->ClearState();

        Log::Comment(L"Direct Cursor Address");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'Y');
        mach.ProcessCharacter(L' ' + 3); // Coordinates must be printable ASCII values,
        mach.ProcessCharacter(L' ' + 5); // so are relative to 0x20 (the space character).
        VERIFY_IS_TRUE(pDispatch->_cursorPosition);
        VERIFY_ARE_EQUAL(3u, pDispatch->_line - 1); // CursorPosition coordinates are 1-based,
        VERIFY_ARE_EQUAL(5u, pDispatch->_column - 1); // so are 1 more than the expected values.

        pDispatch->ClearState();

        Log::Comment(L"Identify Device");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'Z');
        VERIFY_IS_TRUE(pDispatch->_vt52DeviceAttributes);
    }
};
