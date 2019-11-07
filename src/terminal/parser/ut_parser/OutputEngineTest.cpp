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

    virtual void PrintString(const wchar_t* const /*rgwch*/, const size_t /*cch*/) override
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

        unsigned int uiTest;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiTest", uiTest));

        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(L'a');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiEntry)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(L'\x9b');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::CsiEntry);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiImmediate)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        VERIFY_ARE_EQUAL(*mach._pusActiveParam, 12345);
        mach.ProcessCharacter(L'J');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestCsiIgnore)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L'0');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        mach.ProcessCharacter(L';');
        for (int i = 0; i < MAX_PATH; i++) // The buffer is only 256 long, so any longer value should work :P
        {
            mach.ProcessCharacter(L's');
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        }
        VERIFY_ARE_EQUAL(mach._sOscNextChar, mach.s_cOscStringMaxLength - 1);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(NormalTestOscParam)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        VERIFY_ARE_EQUAL(mach._sOscParam, 12345);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestLeadingZeroOscParam)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

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
        VERIFY_ARE_EQUAL(mach._sOscParam, 12345);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestLongOscParam)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        for (int i = 0; i < 6; i++) // We're only expecting to be able to keep 5 digits max
        {
            mach.ProcessCharacter((wchar_t)(L'1' + i));
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._sOscParam, SHORT_MAX);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        Log::Comment(L"Make sure we cap the param value to SHORT_MAX");
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L']');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        for (int i = 0; i < 5; i++) // We're only expecting to be able to keep 5 digits max
        {
            mach.ProcessCharacter((wchar_t)(L'4' + i)); // 45678 > (SHORT_MAX===32767)
            VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscParam);
        }
        VERIFY_ARE_EQUAL(mach._sOscParam, SHORT_MAX);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(L's');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::OscString);
        mach.ProcessCharacter(AsciiChars::BEL);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestSs3Entry)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L'm');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestSs3Immediate)
    {
        // Intermediates aren't supported by Ss3 - they just get dispatched
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L'$');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L'#');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L'%');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L'?');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    }

    TEST_METHOD(TestSs3Param)
    {
        StateMachine mach(new OutputStateMachineEngine(new DummyDispatch));

        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
        mach.ProcessCharacter(AsciiChars::ESC);
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
        mach.ProcessCharacter(L'O');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L'3');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L'2');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L'4');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L';');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L'8');
        VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
        mach.ProcessCharacter(L'J');
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

    virtual void PrintString(const wchar_t* const /*rgwch*/, const size_t /*cch*/) override
    {
    }

    StatefulDispatch() :
        _uiCursorDistance{ 0 },
        _uiLine{ 0 },
        _uiColumn{ 0 },
        _fCursorUp{ false },
        _fCursorDown{ false },
        _fCursorBackward{ false },
        _fCursorForward{ false },
        _fCursorNextLine{ false },
        _fCursorPreviousLine{ false },
        _fCursorHorizontalPositionAbsolute{ false },
        _fVerticalLinePositionAbsolute{ false },
        _fCursorPosition{ false },
        _fCursorSave{ false },
        _fCursorLoad{ false },
        _fCursorVisible{ true },
        _fEraseDisplay{ false },
        _fEraseLine{ false },
        _fInsertCharacter{ false },
        _fDeleteCharacter{ false },
        _eraseType{ (DispatchTypes::EraseType)-1 },
        _fSetGraphics{ false },
        _statusReportType{ (DispatchTypes::AnsiStatusType)-1 },
        _fDeviceStatusReport{ false },
        _fDeviceAttributes{ false },
        _cOptions{ 0 },
        _fIsAltBuffer{ false },
        _fCursorKeysMode{ false },
        _fCursorBlinking{ true },
        _fIsOriginModeRelative{ false },
        _fIsDECCOLMAllowed{ false },
        _uiWindowWidth{ 80 }
    {
        memset(_rgOptions, s_uiGraphicsCleared, sizeof(_rgOptions));
    }

    void ClearState()
    {
        StatefulDispatch dispatch;
        *this = dispatch;
    }

    bool CursorUp(_In_ unsigned int const uiDistance) override
    {
        _fCursorUp = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorDown(_In_ unsigned int const uiDistance) override
    {
        _fCursorDown = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorBackward(_In_ unsigned int const uiDistance) override
    {
        _fCursorBackward = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorForward(_In_ unsigned int const uiDistance) override
    {
        _fCursorForward = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorNextLine(_In_ unsigned int const uiDistance) override
    {
        _fCursorNextLine = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorPrevLine(_In_ unsigned int const uiDistance) override
    {
        _fCursorPreviousLine = true;
        _uiCursorDistance = uiDistance;
        return true;
    }

    bool CursorHorizontalPositionAbsolute(_In_ unsigned int const uiPosition) override
    {
        _fCursorHorizontalPositionAbsolute = true;
        _uiCursorDistance = uiPosition;
        return true;
    }

    bool VerticalLinePositionAbsolute(_In_ unsigned int const uiPosition) override
    {
        _fVerticalLinePositionAbsolute = true;
        _uiCursorDistance = uiPosition;
        return true;
    }

    bool CursorPosition(_In_ unsigned int const uiLine, _In_ unsigned int const uiColumn) override
    {
        _fCursorPosition = true;
        _uiLine = uiLine;
        _uiColumn = uiColumn;
        return true;
    }

    bool CursorSaveState() override
    {
        _fCursorSave = true;
        return true;
    }

    bool CursorRestoreState() override
    {
        _fCursorLoad = true;
        return true;
    }

    bool EraseInDisplay(const DispatchTypes::EraseType eraseType) override
    {
        _fEraseDisplay = true;
        _eraseType = eraseType;
        return true;
    }

    bool EraseInLine(const DispatchTypes::EraseType eraseType) override
    {
        _fEraseLine = true;
        _eraseType = eraseType;
        return true;
    }

    bool InsertCharacter(_In_ unsigned int const uiCount) override
    {
        _fInsertCharacter = true;
        _uiCursorDistance = uiCount;
        return true;
    }

    bool DeleteCharacter(_In_ unsigned int const uiCount) override
    {
        _fDeleteCharacter = true;
        _uiCursorDistance = uiCount;
        return true;
    }

    bool CursorVisibility(const bool fIsVisible) override
    {
        _fCursorVisible = fIsVisible;
        return true;
    }

    bool SetGraphicsRendition(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                              const size_t cOptions) override
    {
        size_t cCopyLength = std::min(cOptions, s_cMaxOptions); // whichever is smaller, our buffer size or the number given
        _cOptions = cCopyLength;
        memcpy(_rgOptions, rgOptions, _cOptions * sizeof(DispatchTypes::GraphicsOptions));

        _fSetGraphics = true;

        return true;
    }

    bool DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType) override
    {
        _fDeviceStatusReport = true;
        _statusReportType = statusType;

        return true;
    }

    bool DeviceAttributes() override
    {
        _fDeviceAttributes = true;

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
        case DispatchTypes::PrivateModeParams::DECCOLM_SetNumberOfColumns:
            fSuccess = SetColumns(static_cast<unsigned int>(fEnable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns));
            break;
        case DispatchTypes::PrivateModeParams::DECOM_OriginMode:
            // The cursor is also moved to the new home position when the origin mode is set or reset.
            fSuccess = SetOriginMode(fEnable) && CursorPosition(1, 1);
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
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
        return fSuccess;
    }

    bool _SetResetPrivateModesHelper(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rParams,
                                     const size_t cParams,
                                     const bool fEnable)
    {
        size_t cFailures = 0;
        for (size_t i = 0; i < cParams; i++)
        {
            cFailures += _PrivateModeParamsHelper(rParams[i], fEnable) ? 0 : 1; // increment the number of failures if we fail.
        }
        return cFailures == 0;
    }

    bool SetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rParams,
                         const size_t cParams) override
    {
        return _SetResetPrivateModesHelper(rParams, cParams, true);
    }

    bool ResetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rParams,
                           const size_t cParams) override
    {
        return _SetResetPrivateModesHelper(rParams, cParams, false);
    }

    bool SetColumns(_In_ unsigned int const uiColumns) override
    {
        _uiWindowWidth = uiColumns;
        return true;
    }

    bool SetVirtualTerminalInputMode(const bool fApplicationMode)
    {
        _fCursorKeysMode = fApplicationMode;
        return true;
    }

    bool EnableCursorBlinking(const bool bEnable) override
    {
        _fCursorBlinking = bEnable;
        return true;
    }

    bool SetOriginMode(const bool fRelativeMode) override
    {
        _fIsOriginModeRelative = fRelativeMode;
        return true;
    }

    bool EnableDECCOLMSupport(const bool fEnabled) override
    {
        _fIsDECCOLMAllowed = fEnabled;
        return true;
    }

    bool UseAlternateScreenBuffer() override
    {
        _fIsAltBuffer = true;
        return true;
    }

    bool UseMainScreenBuffer() override
    {
        _fIsAltBuffer = false;
        return true;
    }

    unsigned int _uiCursorDistance;
    unsigned int _uiLine;
    unsigned int _uiColumn;
    bool _fCursorUp;
    bool _fCursorDown;
    bool _fCursorBackward;
    bool _fCursorForward;
    bool _fCursorNextLine;
    bool _fCursorPreviousLine;
    bool _fCursorHorizontalPositionAbsolute;
    bool _fVerticalLinePositionAbsolute;
    bool _fCursorPosition;
    bool _fCursorSave;
    bool _fCursorLoad;
    bool _fCursorVisible;
    bool _fEraseDisplay;
    bool _fEraseLine;
    bool _fInsertCharacter;
    bool _fDeleteCharacter;
    DispatchTypes::EraseType _eraseType;
    bool _fSetGraphics;
    DispatchTypes::AnsiStatusType _statusReportType;
    bool _fDeviceStatusReport;
    bool _fDeviceAttributes;
    bool _fIsAltBuffer;
    bool _fCursorKeysMode;
    bool _fCursorBlinking;
    bool _fIsOriginModeRelative;
    bool _fIsDECCOLMAllowed;
    unsigned int _uiWindowWidth;

    static const size_t s_cMaxOptions = 16;
    static const unsigned int s_uiGraphicsCleared = UINT_MAX;
    DispatchTypes::GraphicsOptions _rgOptions[s_cMaxOptions];
    size_t _cOptions;
};

class StateMachineExternalTest final
{
    TEST_CLASS(StateMachineExternalTest);

    TEST_METHOD_SETUP(SetupState)
    {
        return true;
    }

    void TestEscCursorMovement(wchar_t const wchCommand,
                               const bool* const pfFlag,
                               StateMachine& mach,
                               StatefulDispatch& dispatch)
    {
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(wchCommand);

        VERIFY_IS_TRUE(*pfFlag);
        VERIFY_ARE_EQUAL(dispatch._uiCursorDistance, 1u);
    }

    TEST_METHOD(TestEscCursorMovement)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        TestEscCursorMovement(L'A', &pDispatch->_fCursorUp, mach, *pDispatch);
        TestEscCursorMovement(L'B', &pDispatch->_fCursorDown, mach, *pDispatch);
        TestEscCursorMovement(L'C', &pDispatch->_fCursorForward, mach, *pDispatch);
        TestEscCursorMovement(L'D', &pDispatch->_fCursorBackward, mach, *pDispatch);
    }

    void InsertNumberToMachine(StateMachine* const pMachine, unsigned int uiNumber)
    {
        static const size_t cchBufferMax = 20;

        wchar_t pwszDistance[cchBufferMax];
        int cchDistance = swprintf_s(pwszDistance, cchBufferMax, L"%d", uiNumber);

        if (cchDistance > 0 && cchDistance < cchBufferMax)
        {
            for (int i = 0; i < cchDistance; i++)
            {
                pMachine->ProcessCharacter(pwszDistance[i]);
            }
        }
    }

    void ApplyParameterBoundary(unsigned int* uiExpected, unsigned int uiGiven)
    {
        // 0 and 1 should be 1. Use the preset value.
        // 1-SHORT_MAX should be what we set.
        // > SHORT_MAX should be SHORT_MAX.
        if (uiGiven <= 1)
        {
            *uiExpected = 1u;
        }
        else if (uiGiven > 1 && uiGiven <= SHORT_MAX)
        {
            *uiExpected = uiGiven;
        }
        else if (uiGiven > SHORT_MAX)
        {
            *uiExpected = SHORT_MAX; // 16383 is our max value.
        }
    }

    void TestCsiCursorMovement(wchar_t const wchCommand,
                               unsigned int const uiDistance,
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

        unsigned int uiExpectedDistance = 1u;

        if (fUseDistance)
        {
            ApplyParameterBoundary(&uiExpectedDistance, uiDistance);
        }

        VERIFY_ARE_EQUAL(dispatch._uiCursorDistance, uiExpectedDistance);
    }

    TEST_METHOD(TestCsiCursorMovementWithValues)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDistance", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        unsigned int uiDistance;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDistance", uiDistance));

        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        TestCsiCursorMovement(L'A', uiDistance, true, &pDispatch->_fCursorUp, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'B', uiDistance, true, &pDispatch->_fCursorDown, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'C', uiDistance, true, &pDispatch->_fCursorForward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'D', uiDistance, true, &pDispatch->_fCursorBackward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'E', uiDistance, true, &pDispatch->_fCursorNextLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'F', uiDistance, true, &pDispatch->_fCursorPreviousLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'G', uiDistance, true, &pDispatch->_fCursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'`', uiDistance, true, &pDispatch->_fCursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'd', uiDistance, true, &pDispatch->_fVerticalLinePositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'@', uiDistance, true, &pDispatch->_fInsertCharacter, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'P', uiDistance, true, &pDispatch->_fDeleteCharacter, mach, *pDispatch);
    }

    TEST_METHOD(TestCsiCursorMovementWithoutValues)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        unsigned int uiDistance = 9999; // this value should be ignored with the false below.
        TestCsiCursorMovement(L'A', uiDistance, false, &pDispatch->_fCursorUp, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'B', uiDistance, false, &pDispatch->_fCursorDown, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'C', uiDistance, false, &pDispatch->_fCursorForward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'D', uiDistance, false, &pDispatch->_fCursorBackward, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'E', uiDistance, false, &pDispatch->_fCursorNextLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'F', uiDistance, false, &pDispatch->_fCursorPreviousLine, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'G', uiDistance, false, &pDispatch->_fCursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'`', uiDistance, false, &pDispatch->_fCursorHorizontalPositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'd', uiDistance, false, &pDispatch->_fVerticalLinePositionAbsolute, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'@', uiDistance, false, &pDispatch->_fInsertCharacter, mach, *pDispatch);
        pDispatch->ClearState();
        TestCsiCursorMovement(L'P', uiDistance, false, &pDispatch->_fDeleteCharacter, mach, *pDispatch);
    }

    TEST_METHOD(TestCsiCursorPosition)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiRow", PARAM_VALUES)
            TEST_METHOD_PROPERTY(L"Data:uiCol", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        unsigned int uiRow;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiRow", uiRow));
        unsigned int uiCol;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiCol", uiCol));

        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        InsertNumberToMachine(&mach, uiRow);
        mach.ProcessCharacter(L';');
        InsertNumberToMachine(&mach, uiCol);
        mach.ProcessCharacter(L'H');

        // bound the row/col values by the max we expect
        ApplyParameterBoundary(&uiRow, uiRow);
        ApplyParameterBoundary(&uiCol, uiCol);

        VERIFY_IS_TRUE(pDispatch->_fCursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_uiLine, uiRow);
        VERIFY_ARE_EQUAL(pDispatch->_uiColumn, uiCol);
    }

    TEST_METHOD(TestCsiCursorPositionWithOnlyRow)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiRow", PARAM_VALUES)
        END_TEST_METHOD_PROPERTIES()

        unsigned int uiRow;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiRow", uiRow));

        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');

        InsertNumberToMachine(&mach, uiRow);
        mach.ProcessCharacter(L'H');

        // bound the row/col values by the max we expect
        ApplyParameterBoundary(&uiRow, uiRow);

        VERIFY_IS_TRUE(pDispatch->_fCursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_uiLine, uiRow);
        VERIFY_ARE_EQUAL(pDispatch->_uiColumn, (unsigned int)1); // Without the second param, the column should always be the default
    }

    TEST_METHOD(TestCursorSaveLoad)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'7');
        VERIFY_IS_TRUE(pDispatch->_fCursorSave);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'8');
        VERIFY_IS_TRUE(pDispatch->_fCursorLoad);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L's');
        VERIFY_IS_TRUE(pDispatch->_fCursorSave);

        pDispatch->ClearState();

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'u');
        VERIFY_IS_TRUE(pDispatch->_fCursorLoad);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorKeysMode)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?1h", 5);
        VERIFY_IS_TRUE(pDispatch->_fCursorKeysMode);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1l", 5);
        VERIFY_IS_FALSE(pDispatch->_fCursorKeysMode);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestSetNumberOfColumns)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?3h", 5);
        VERIFY_ARE_EQUAL(pDispatch->_uiWindowWidth, static_cast<unsigned int>(DispatchTypes::s_sDECCOLMSetColumns));

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?3l", 5);
        VERIFY_ARE_EQUAL(pDispatch->_uiWindowWidth, static_cast<unsigned int>(DispatchTypes::s_sDECCOLMResetColumns));

        pDispatch->ClearState();
    }

    TEST_METHOD(TestOriginMode)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?6h", 5);
        VERIFY_IS_TRUE(pDispatch->_fIsOriginModeRelative);
        VERIFY_IS_TRUE(pDispatch->_fCursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_uiLine, 1u);
        VERIFY_ARE_EQUAL(pDispatch->_uiColumn, 1u);

        pDispatch->ClearState();
        pDispatch->_fIsOriginModeRelative = true;

        mach.ProcessString(L"\x1b[?6l", 5);
        VERIFY_IS_FALSE(pDispatch->_fIsOriginModeRelative);
        VERIFY_IS_TRUE(pDispatch->_fCursorPosition);
        VERIFY_ARE_EQUAL(pDispatch->_uiLine, 1u);
        VERIFY_ARE_EQUAL(pDispatch->_uiColumn, 1u);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorBlinking)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?12h", 6);
        VERIFY_IS_TRUE(pDispatch->_fCursorBlinking);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?12l", 6);
        VERIFY_IS_FALSE(pDispatch->_fCursorBlinking);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestCursorVisibility)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?25h", 6);
        VERIFY_IS_TRUE(pDispatch->_fCursorVisible);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?25l", 6);
        VERIFY_IS_FALSE(pDispatch->_fCursorVisible);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestAltBufferSwapping)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?1049h", 8);
        VERIFY_IS_TRUE(pDispatch->_fIsAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049h", 8);
        VERIFY_IS_TRUE(pDispatch->_fIsAltBuffer);
        mach.ProcessString(L"\x1b[?1049h", 8);
        VERIFY_IS_TRUE(pDispatch->_fIsAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049l", 8);
        VERIFY_IS_FALSE(pDispatch->_fIsAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049h", 8);
        VERIFY_IS_TRUE(pDispatch->_fIsAltBuffer);
        mach.ProcessString(L"\x1b[?1049l", 8);
        VERIFY_IS_FALSE(pDispatch->_fIsAltBuffer);

        pDispatch->ClearState();

        mach.ProcessString(L"\x1b[?1049l", 8);
        VERIFY_IS_FALSE(pDispatch->_fIsAltBuffer);
        mach.ProcessString(L"\x1b[?1049l", 8);
        VERIFY_IS_FALSE(pDispatch->_fIsAltBuffer);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestEnableDECCOLMSupport)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        mach.ProcessString(L"\x1b[?40h");
        VERIFY_IS_TRUE(pDispatch->_fIsDECCOLMAllowed);

        pDispatch->ClearState();
        pDispatch->_fIsDECCOLMAllowed = true;

        mach.ProcessString(L"\x1b[?40l");
        VERIFY_IS_FALSE(pDispatch->_fIsDECCOLMAllowed);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestErase)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiEraseOperation", L"{0, 1}") // for "display" and "line" type erase operations
            TEST_METHOD_PROPERTY(L"Data:uiDispatchTypes::EraseType", L"{0, 1, 2, 10}") // maps to DispatchTypes::EraseType enum class options.
        END_TEST_METHOD_PROPERTIES()

        unsigned int uiEraseOperation;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiEraseOperation", uiEraseOperation));
        unsigned int uiDispatchTypes;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDispatchTypes::EraseType", uiDispatchTypes));

        WCHAR wchOp = L'\0';
        bool* pfOperationCallback = nullptr;

        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        switch (uiEraseOperation)
        {
        case 0:
            wchOp = L'J';
            pfOperationCallback = &pDispatch->_fEraseDisplay;
            break;
        case 1:
            wchOp = L'K';
            pfOperationCallback = &pDispatch->_fEraseLine;
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

    void VerifyDispatchTypes(_In_reads_(cExpectedOptions) const DispatchTypes::GraphicsOptions* const rgExpectedOptions,
                             const size_t cExpectedOptions,
                             const StatefulDispatch& dispatch)
    {
        VERIFY_ARE_EQUAL(cExpectedOptions, dispatch._cOptions);
        bool fOptionsValid = true;

        for (size_t i = 0; i < dispatch.s_cMaxOptions; i++)
        {
            auto expectedOption = (DispatchTypes::GraphicsOptions)dispatch.s_uiGraphicsCleared;

            if (i < cExpectedOptions)
            {
                expectedOption = rgExpectedOptions[i];
            }

            fOptionsValid = expectedOption == dispatch._rgOptions[i];

            if (!fOptionsValid)
            {
                Log::Comment(NoThrowString().Format(L"Graphics option match failed, index [%zu]. Expected: '%d' Actual: '%d'", i, expectedOption, dispatch._rgOptions[i]));
                break;
            }
        }

        VERIFY_IS_TRUE(fOptionsValid);
    }

    TEST_METHOD(TestSetGraphicsRendition)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        DispatchTypes::GraphicsOptions rgExpected[16];

        Log::Comment(L"Test 1: Check default case.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes(rgExpected, 1, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check clear/0 case.");

        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'0');
        mach.ProcessCharacter(L'm');
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes(rgExpected, 1, *pDispatch);

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
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[2] = DispatchTypes::GraphicsOptions::Negative;
        rgExpected[3] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        rgExpected[4] = DispatchTypes::GraphicsOptions::BackgroundMagenta;
        VerifyDispatchTypes(rgExpected, 5, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 4: Check 'too many options' (>16) case.");

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
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

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
        VerifyDispatchTypes(rgExpected, 16, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.a: Test an empty param at the end of a sequence");

        std::wstring sequence = L"\x1b[1;m";
        mach.ProcessString(&sequence[0], sequence.length());
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Off;
        VerifyDispatchTypes(rgExpected, 2, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.b: Test an empty param in the middle of a sequence");

        sequence = L"\x1b[1;;1m";
        mach.ProcessString(&sequence[0], sequence.length());
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Off;
        rgExpected[2] = DispatchTypes::GraphicsOptions::BoldBright;
        VerifyDispatchTypes(rgExpected, 3, *pDispatch);

        pDispatch->ClearState();

        Log::Comment(L"Test 5.c: Test an empty param at the start of a sequence");

        sequence = L"\x1b[;31;1m";
        mach.ProcessString(&sequence[0], sequence.length());
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        rgExpected[0] = DispatchTypes::GraphicsOptions::Off;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundRed;
        rgExpected[2] = DispatchTypes::GraphicsOptions::BoldBright;
        VerifyDispatchTypes(rgExpected, 3, *pDispatch);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestDeviceStatusReport)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        Log::Comment(L"Test 1: Check empty case. Should fail.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_FALSE(pDispatch->_fDeviceStatusReport);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check CSR (cursor position command) case 6. Should succeed.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'6');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_TRUE(pDispatch->_fDeviceStatusReport);
        VERIFY_ARE_EQUAL(DispatchTypes::AnsiStatusType::CPR_CursorPositionReport, pDispatch->_statusReportType);

        pDispatch->ClearState();

        Log::Comment(L"Test 3: Check unimplemented case 1. Should fail.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L'n');

        VERIFY_IS_FALSE(pDispatch->_fDeviceStatusReport);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestDeviceAttributes)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        Log::Comment(L"Test 1: Check default case, no params.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_TRUE(pDispatch->_fDeviceAttributes);

        pDispatch->ClearState();

        Log::Comment(L"Test 2: Check default case, 0 param.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'0');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_TRUE(pDispatch->_fDeviceAttributes);

        pDispatch->ClearState();

        Log::Comment(L"Test 3: Check fail case, 1 (or any other) param.");
        mach.ProcessCharacter(AsciiChars::ESC);
        mach.ProcessCharacter(L'[');
        mach.ProcessCharacter(L'1');
        mach.ProcessCharacter(L'c');

        VERIFY_IS_FALSE(pDispatch->_fDeviceAttributes);

        pDispatch->ClearState();
    }

    TEST_METHOD(TestStrings)
    {
        StatefulDispatch* pDispatch = new StatefulDispatch;
        VERIFY_IS_NOT_NULL(pDispatch);
        StateMachine mach(new OutputStateMachineEngine(pDispatch));

        DispatchTypes::GraphicsOptions rgExpected[16];
        DispatchTypes::EraseType expectedDispatchTypes;
        ///////////////////////////////////////////////////////////////////////

        Log::Comment(L"Test 1: Basic String processing. One sequence in a string.");
        mach.ProcessString(L"\x1b[0m", 4);

        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////

        Log::Comment(L"Test 2: A couple of sequences all in one string");

        mach.ProcessString(L"\x1b[1;4;7;30;45m\x1b[2J", 18);
        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);
        VERIFY_IS_TRUE(pDispatch->_fEraseDisplay);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::Underline;
        rgExpected[2] = DispatchTypes::GraphicsOptions::Negative;
        rgExpected[3] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        rgExpected[4] = DispatchTypes::GraphicsOptions::BackgroundMagenta;
        expectedDispatchTypes = DispatchTypes::EraseType::All;
        VerifyDispatchTypes(rgExpected, 5, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 3: Two sequences seperated by a non-sequence of characters");

        mach.ProcessString(L"\x1b[1;30mHello World\x1b[2J", 22);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);
        VERIFY_IS_TRUE(pDispatch->_fEraseDisplay);

        VerifyDispatchTypes(rgExpected, 2, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 4: An entire sequence broke into multiple strings");
        mach.ProcessString(L"\x1b[1;", 4);
        VERIFY_IS_FALSE(pDispatch->_fSetGraphics);
        VERIFY_IS_FALSE(pDispatch->_fEraseDisplay);

        mach.ProcessString(L"30mHello World\x1b[2J", 18);

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;
        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);
        VERIFY_IS_TRUE(pDispatch->_fEraseDisplay);

        VerifyDispatchTypes(rgExpected, 2, *pDispatch);
        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();

        ///////////////////////////////////////////////////////////////////////
        Log::Comment(L"Test 5: A sequence with mixed ProcessCharacter and ProcessString calls");

        rgExpected[0] = DispatchTypes::GraphicsOptions::BoldBright;
        rgExpected[1] = DispatchTypes::GraphicsOptions::ForegroundBlack;

        mach.ProcessString(L"\x1b[1;", 4);
        VERIFY_IS_FALSE(pDispatch->_fSetGraphics);
        VERIFY_IS_FALSE(pDispatch->_fEraseDisplay);

        mach.ProcessCharacter(L'3');
        VERIFY_IS_FALSE(pDispatch->_fSetGraphics);
        VERIFY_IS_FALSE(pDispatch->_fEraseDisplay);

        mach.ProcessCharacter(L'0');
        VERIFY_IS_FALSE(pDispatch->_fSetGraphics);
        VERIFY_IS_FALSE(pDispatch->_fEraseDisplay);

        mach.ProcessCharacter(L'm');

        VERIFY_IS_TRUE(pDispatch->_fSetGraphics);
        VERIFY_IS_FALSE(pDispatch->_fEraseDisplay);
        VerifyDispatchTypes(rgExpected, 2, *pDispatch);

        mach.ProcessString(L"Hello World\x1b[2J", 15);

        expectedDispatchTypes = DispatchTypes::EraseType::All;

        VERIFY_IS_TRUE(pDispatch->_fEraseDisplay);

        VERIFY_ARE_EQUAL(expectedDispatchTypes, pDispatch->_eraseType);

        pDispatch->ClearState();
    }
};
