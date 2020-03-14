// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "stateMachine.hpp"
#include "InputStateMachineEngine.hpp"
#include "../input/terminalInput.hpp"
#include "../../inc/unicode.hpp"
#include "../../types/inc/convert.hpp"

#include <vector>
#include <functional>
#include <sstream>
#include <string>
#include <algorithm>

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../../interactivity/inc/VtApiRedirection.hpp"
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
            class InputEngineTest;
            class TestInteractDispatch;
        };
    };
};
using namespace Microsoft::Console::VirtualTerminal;

bool IsShiftPressed(const DWORD modifierState)
{
    return WI_IsFlagSet(modifierState, SHIFT_PRESSED);
}

bool IsAltPressed(const DWORD modifierState)
{
    return WI_IsAnyFlagSet(modifierState, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
}

bool IsCtrlPressed(const DWORD modifierState)
{
    return WI_IsAnyFlagSet(modifierState, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
}

bool ModifiersEquivalent(DWORD a, DWORD b)
{
    bool fShift = IsShiftPressed(a) == IsShiftPressed(b);
    bool fAlt = IsAltPressed(a) == IsAltPressed(b);
    bool fCtrl = IsCtrlPressed(a) == IsCtrlPressed(b);
    return fShift && fCtrl && fAlt;
}

class TestState
{
public:
    TestState() :
        vExpectedInput{},
        _stateMachine{ nullptr },
        _expectedToCallWindowManipulation{ false },
        _expectSendCtrlC{ false },
        _expectCursorPosition{ false },
        _expectedCursor{ -1, -1 },
        _expectedWindowManipulation{ DispatchTypes::WindowManipulationType::Invalid },
        _expectedCParams{ 0 }
    {
        std::fill_n(_expectedParams, ARRAYSIZE(_expectedParams), gsl::narrow<short>(0));
    }

    void RoundtripTerminalInputCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        // Take all the characters out of the input records here, and put them into
        //  the input state machine.
        auto inputRecords = IInputEvent::ToInputRecords(inEvents);
        std::wstring vtseq = L"";
        for (auto& inRec : inputRecords)
        {
            VERIFY_ARE_EQUAL(KEY_EVENT, inRec.EventType);
            if (inRec.Event.KeyEvent.bKeyDown)
            {
                vtseq += &inRec.Event.KeyEvent.uChar.UnicodeChar;
            }
        }
        Log::Comment(
            NoThrowString().Format(L"\tvtseq: \"%s\"(%zu)", vtseq.c_str(), vtseq.length()));

        _stateMachine->ProcessString(vtseq);
        Log::Comment(L"String processed");
    }

    void TestInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        auto records = IInputEvent::ToInputRecords(inEvents);
        VERIFY_ARE_EQUAL((size_t)1, vExpectedInput.size());

        bool foundEqual = false;
        INPUT_RECORD irExpected = vExpectedInput.back();

        Log::Comment(
            NoThrowString().Format(L"\texpected:\t") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));

        // Look for an equivalent input record.
        // Differences between left and right modifiers are ignored, as long as one is pressed.
        // There may be other keypresses, eg. modifier keypresses, those are ignored.
        for (auto& inRec : records)
        {
            Log::Comment(
                NoThrowString().Format(L"\tActual  :\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(inRec));

            bool areEqual =
                (irExpected.EventType == inRec.EventType) &&
                (irExpected.Event.KeyEvent.bKeyDown == inRec.Event.KeyEvent.bKeyDown) &&
                (irExpected.Event.KeyEvent.wRepeatCount == inRec.Event.KeyEvent.wRepeatCount) &&
                (irExpected.Event.KeyEvent.uChar.UnicodeChar == inRec.Event.KeyEvent.uChar.UnicodeChar) &&
                ModifiersEquivalent(irExpected.Event.KeyEvent.dwControlKeyState, inRec.Event.KeyEvent.dwControlKeyState);

            foundEqual |= areEqual;
            if (areEqual)
            {
                Log::Comment(L"\t\tFound Match");
            }
        }

        VERIFY_IS_TRUE(foundEqual);
        vExpectedInput.clear();
    }

    void TestInputStringCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        auto records = IInputEvent::ToInputRecords(inEvents);

        for (auto expected : vExpectedInput)
        {
            Log::Comment(
                NoThrowString().Format(L"\texpected:\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(expected));
        }

        INPUT_RECORD irExpected = vExpectedInput.front();
        Log::Comment(
            NoThrowString().Format(L"\tLooking for:\t") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));

        // Look for an equivalent input record.
        // Differences between left and right modifiers are ignored, as long as one is pressed.
        // There may be other keypresses, eg. modifier keypresses, those are ignored.
        for (auto& inRec : records)
        {
            Log::Comment(
                NoThrowString().Format(L"\tActual  :\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(inRec));

            bool areEqual =
                (irExpected.EventType == inRec.EventType) &&
                (irExpected.Event.KeyEvent.bKeyDown == inRec.Event.KeyEvent.bKeyDown) &&
                (irExpected.Event.KeyEvent.wRepeatCount == inRec.Event.KeyEvent.wRepeatCount) &&
                (irExpected.Event.KeyEvent.uChar.UnicodeChar == inRec.Event.KeyEvent.uChar.UnicodeChar) &&
                ModifiersEquivalent(irExpected.Event.KeyEvent.dwControlKeyState, inRec.Event.KeyEvent.dwControlKeyState);

            if (areEqual)
            {
                Log::Comment(L"\t\tFound Match");
                vExpectedInput.pop_front();
                if (vExpectedInput.size() > 0)
                {
                    irExpected = vExpectedInput.front();
                    Log::Comment(
                        NoThrowString().Format(L"\tLooking for:\t") +
                        VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));
                }
            }
        }
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), vExpectedInput.size(), L"Verify we found all the inputs we were expecting");
        vExpectedInput.clear();
    }

    std::deque<INPUT_RECORD> vExpectedInput;
    StateMachine* _stateMachine;
    bool _expectedToCallWindowManipulation;
    bool _expectSendCtrlC;
    bool _expectCursorPosition;
    COORD _expectedCursor;
    DispatchTypes::WindowManipulationType _expectedWindowManipulation;
    unsigned short _expectedParams[16];
    size_t _expectedCParams;
};

class Microsoft::Console::VirtualTerminal::InputEngineTest
{
    TEST_CLASS(InputEngineTest);

    TestState testState;

    void RoundtripTerminalInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    void TestInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    void TestInputStringCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    std::wstring GenerateSgrMouseSequence(const CsiMouseButtonCodes button,
                                          const unsigned short modifiers,
                                          const COORD position,
                                          const CsiActionCodes direction);

    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiActionMouseCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)
    typedef std::tuple<CsiMouseButtonCodes, unsigned short, COORD, CsiActionCodes> SGR_PARAMS;

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags
    typedef std::tuple<DWORD, DWORD, COORD, DWORD> MOUSE_EVENT_PARAMS;

    void VerifySGRMouseData(const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData);

    // We need to manually call this at the end of the tests so that we know _which_ tests failed, rather than that the method cleanup failed
    void VerifyExpectedInputDrained();

    TEST_CLASS_SETUP(ClassSetup)
    {
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        return true;
    }

    TEST_METHOD(C0Test);
    TEST_METHOD(AlphanumericTest);
    TEST_METHOD(RoundTripTest);
    TEST_METHOD(WindowManipulationTest);
    TEST_METHOD(NonAsciiTest);
    TEST_METHOD(CursorPositioningTest);
    TEST_METHOD(CSICursorBackTabTest);
    TEST_METHOD(AltBackspaceTest);
    TEST_METHOD(AltCtrlDTest);
    TEST_METHOD(AltIntermediateTest);
    TEST_METHOD(AltBackspaceEnterTest);
    TEST_METHOD(SGRMouseTest_ButtonClick);
    TEST_METHOD(SGRMouseTest_Modifiers);
    TEST_METHOD(SGRMouseTest_Movement);
    TEST_METHOD(SGRMouseTest_Scroll);

    friend class TestInteractDispatch;
};

void InputEngineTest::VerifyExpectedInputDrained()
{
    if (!testState.vExpectedInput.empty())
    {
        for (const auto& exp : testState.vExpectedInput)
        {
            switch (exp.EventType)
            {
            case KEY_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: KEY_EVENT");
                break;
            case MOUSE_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: MOUSE_EVENT");
                break;
            case WINDOW_BUFFER_SIZE_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: WINDOW_BUFFER_SIZE_EVENT");
                break;
            case MENU_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: MENU_EVENT");
                break;
            case FOCUS_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: FOCUS_EVENT");
                break;
            default:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: UNKNOWN TYPE");
                break;
            }
        }
        VERIFY_FAIL(L"there should be no remaining un-drained expected input");
        testState.vExpectedInput.clear();
    }
}

class Microsoft::Console::VirtualTerminal::TestInteractDispatch final : public IInteractDispatch
{
public:
    TestInteractDispatch(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn,
                         _In_ TestState* testState);
    virtual bool WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents) override;
    virtual bool WriteCtrlC() override;
    virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                    const std::basic_string_view<size_t> parameters) override; // DTTERM_WindowManipulation
    virtual bool WriteString(const std::wstring_view string) override;

    virtual bool MoveCursor(const size_t row,
                            const size_t col) override;

    virtual bool IsVtInputEnabled() const override;

private:
    std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteInputCallback;
    TestState* _testState; // non-ownership pointer
};

TestInteractDispatch::TestInteractDispatch(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn,
                                           _In_ TestState* testState) :
    _pfnWriteInputCallback(pfn),
    _testState(testState)
{
}

bool TestInteractDispatch::WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents)
{
    _pfnWriteInputCallback(inputEvents);
    return true;
}

bool TestInteractDispatch::WriteCtrlC()
{
    VERIFY_IS_TRUE(_testState->_expectSendCtrlC);
    KeyEvent key = KeyEvent(true, 1, 'C', 0, UNICODE_ETX, LEFT_CTRL_PRESSED);
    std::deque<std::unique_ptr<IInputEvent>> inputEvents;
    inputEvents.push_back(std::make_unique<KeyEvent>(key));
    return WriteInput(inputEvents);
}

bool TestInteractDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                              const std::basic_string_view<size_t> parameters)
{
    VERIFY_ARE_EQUAL(true, _testState->_expectedToCallWindowManipulation);
    VERIFY_ARE_EQUAL(_testState->_expectedWindowManipulation, function);
    for (size_t i = 0; i < parameters.size(); i++)
    {
        unsigned short actual;
        VERIFY_SUCCEEDED(SizeTToUShort(parameters.at(i), &actual));
        VERIFY_ARE_EQUAL(_testState->_expectedParams[i], actual);
    }
    return true;
}

bool TestInteractDispatch::WriteString(const std::wstring_view string)
{
    std::deque<std::unique_ptr<IInputEvent>> keyEvents;

    for (const auto& wch : string)
    {
        // We're forcing the translation to CP_USA, so that it'll be constant
        //  regardless of the CP the test is running in
        std::deque<std::unique_ptr<KeyEvent>> convertedEvents = CharToKeyEvents(wch, CP_USA);
        std::move(convertedEvents.begin(),
                  convertedEvents.end(),
                  std::back_inserter(keyEvents));
    }

    return WriteInput(keyEvents);
}

bool TestInteractDispatch::MoveCursor(const size_t row, const size_t col)
{
    VERIFY_IS_TRUE(_testState->_expectCursorPosition);
    COORD received = { static_cast<short>(col), static_cast<short>(row) };
    VERIFY_ARE_EQUAL(_testState->_expectedCursor, received);
    return true;
}

bool TestInteractDispatch::IsVtInputEnabled() const
{
    return true;
}

void InputEngineTest::C0Test()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(L"Sending 0x0-0x19 to parser to make sure they're translated correctly back to C-key");
    DisableVerifyExceptions disable;
    for (wchar_t wch = '\x0'; wch < '\x20'; wch++)
    {
        std::wstring inputSeq = std::wstring(&wch, 1);
        // In general, he actual key that we're going to generate for a C0 char
        //      is char+0x40 and with ctrl pressed.
        wchar_t expectedWch = wch + 0x40;
        bool writeCtrl = true;
        // These two are weird exceptional cases.
        switch (wch)
        {
        case L'\r': // Enter
            expectedWch = wch;
            writeCtrl = false;
            break;
        case L'\x1b': // Escape
            expectedWch = wch;
            writeCtrl = false;
            break;
        case L'\t': // Tab
            writeCtrl = false;
            break;
        case L'\b': // backspace
            wch = '\x7f';
            expectedWch = '\x7f';
            break;
        }

        short keyscan = VkKeyScanW(expectedWch);
        short vkey = keyscan & 0xff;
        short keyscanModifiers = (keyscan >> 8) & 0xff;
        WORD scanCode = (WORD)MapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        DWORD dwModifierState = 0;
        if (writeCtrl)
        {
            dwModifierState = WI_SetFlag(dwModifierState, LEFT_CTRL_PRESSED);
        }
        // If we need to press shift for this key, but not on alphabetical chars
        //  Eg simulating C-z, not C-S-z.
        if (WI_IsFlagSet(keyscanModifiers, 1) && (expectedWch < L'A' || expectedWch > L'Z'))
        {
            dwModifierState = WI_SetFlag(dwModifierState, SHIFT_PRESSED);
        }

        // Just make sure we write the same thing telnetd did:
        if (wch == UNICODE_ETX)
        {
            Log::Comment(NoThrowString().Format(
                L"We used to expect 0x%x, 0x%x, 0x%x, 0x%x here",
                vkey,
                scanCode,
                wch,
                dwModifierState));
            vkey = 'C';
            scanCode = 0;
            wch = UNICODE_ETX;
            dwModifierState = LEFT_CTRL_PRESSED;
            Log::Comment(NoThrowString().Format(
                L"Now we expect 0x%x, 0x%x, 0x%x, 0x%x here",
                vkey,
                scanCode,
                wch,
                dwModifierState));
            testState._expectSendCtrlC = true;
        }
        else
        {
            testState._expectSendCtrlC = false;
        }

        Log::Comment(NoThrowString().Format(L"Testing char 0x%x", wch));
        Log::Comment(NoThrowString().Format(L"Input Sequence=\"%s\"", inputSeq.c_str()));

        INPUT_RECORD inputRec;

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = dwModifierState;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = wch;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::AlphanumericTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(L"Sending every printable ASCII character");
    DisableVerifyExceptions disable;
    for (wchar_t wch = '\x20'; wch < '\x7f'; wch++)
    {
        std::wstring inputSeq = std::wstring(&wch, 1);

        short keyscan = VkKeyScanW(wch);
        short vkey = keyscan & 0xff;
        WORD scanCode = (wchar_t)MapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        short keyscanModifiers = (keyscan >> 8) & 0xff;
        // Because of course, these are not the same flags.
        DWORD dwModifierState = 0 |
                                (WI_IsFlagSet(keyscanModifiers, 1) ? SHIFT_PRESSED : 0) |
                                (WI_IsFlagSet(keyscanModifiers, 2) ? LEFT_CTRL_PRESSED : 0) |
                                (WI_IsFlagSet(keyscanModifiers, 4) ? LEFT_ALT_PRESSED : 0);

        Log::Comment(NoThrowString().Format(L"Testing char 0x%x", wch));
        Log::Comment(NoThrowString().Format(L"Input Sequence=\"%s\"", inputSeq.c_str()));

        INPUT_RECORD inputRec;
        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = dwModifierState;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = wch;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::RoundTripTest()
{
    // TODO GH #4405: This test fails.
    Log::Result(WEX::Logging::TestResults::Skipped);
    return;

    /*
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    // Send Every VKEY through the TerminalInput module, then take the char's
    //   from the generated INPUT_RECORDs and put them through the InputEngine.
    // The VKEY sequence it writes out should be the same as the original.

    auto pfn2 = std::bind(&TestState::RoundtripTerminalInputCallback, &testState, std::placeholders::_1);
    TerminalInput terminalInput{ pfn2 };

    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        wchar_t wch = (wchar_t)MapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR);
        WORD scanCode = (wchar_t)MapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        unsigned int uiActualKeystate = 0;

        // Couple of exceptional cases here:
        if (vkey >= 'A' && vkey <= 'Z')
        {
            // A-Z need shift pressed in addition to the 'a'-'z' chars.
            uiActualKeystate = WI_SetFlag(uiActualKeystate, SHIFT_PRESSED);
        }
        else if (vkey == VK_CANCEL || vkey == VK_PAUSE)
        {
            uiActualKeystate = WI_SetFlag(uiActualKeystate, LEFT_CTRL_PRESSED);
        }

        if (vkey == UNICODE_ETX)
        {
            testState._expectSendCtrlC = true;
        }

        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.dwControlKeyState = uiActualKeystate;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = wch;
        irTest.Event.KeyEvent.wVirtualScanCode = scanCode;

        Log::Comment(
            NoThrowString().Format(L"Expecting::   ") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irTest));

        testState.vExpectedInput.clear();
        testState.vExpectedInput.push_back(irTest);

        auto inputKey = IInputEvent::Create(irTest);
        terminalInput.HandleKey(inputKey.get());
    }

    VerifyExpectedInputDrained();
    */
}

void InputEngineTest::WindowManipulationTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine.get());
    testState._stateMachine = _stateMachine.get();

    Log::Comment(NoThrowString().Format(
        L"Try sending a bunch of Window Manipulation sequences. "
        L"Only the valid ones should call the "
        L"TestInteractDispatch::WindowManipulation callback."));

    bool fValidType = false;

    const unsigned short param1 = 123;
    const unsigned short param2 = 456;
    const wchar_t* const wszParam1 = L"123";
    const wchar_t* const wszParam2 = L"456";

    for (unsigned int i = 0; i < static_cast<unsigned int>(BYTE_MAX); i++)
    {
        if (i == DispatchTypes::WindowManipulationType::ResizeWindowInCharacters)
        {
            fValidType = true;
        }

        std::wstringstream seqBuilder;
        seqBuilder << L"\x1b[" << i;

        if (i == DispatchTypes::WindowManipulationType::ResizeWindowInCharacters)
        {
            // We need to build the string with the params as strings for some reason -
            //      x86 would implicitly convert them to chars (eg 123 -> '{')
            //      before appending them to the string
            seqBuilder << L";" << wszParam1 << L";" << wszParam2;

            testState._expectedToCallWindowManipulation = true;
            testState._expectedCParams = 2;
            testState._expectedParams[0] = param1;
            testState._expectedParams[1] = param2;
            testState._expectedWindowManipulation = static_cast<DispatchTypes::WindowManipulationType>(i);
        }
        else if (i == DispatchTypes::WindowManipulationType::RefreshWindow)
        {
            // refresh window doesn't expect any params.

            testState._expectedToCallWindowManipulation = true;
            testState._expectedCParams = 0;
            testState._expectedWindowManipulation = static_cast<DispatchTypes::WindowManipulationType>(i);
        }
        else
        {
            testState._expectedToCallWindowManipulation = false;
            testState._expectedCParams = 0;
            testState._expectedWindowManipulation = DispatchTypes::WindowManipulationType::Invalid;
        }
        seqBuilder << L"t";
        std::wstring seq = seqBuilder.str();
        Log::Comment(NoThrowString().Format(
            L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }
}

void InputEngineTest::NonAsciiTest()
{
    auto pfn = std::bind(&TestState::TestInputStringCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine.get());
    testState._stateMachine = _stateMachine.get();
    Log::Comment(L"Sending various non-ascii strings, and seeing what we get out");

    INPUT_RECORD proto = { 0 };
    proto.EventType = KEY_EVENT;
    proto.Event.KeyEvent.dwControlKeyState = 0;
    proto.Event.KeyEvent.wRepeatCount = 1;
    proto.Event.KeyEvent.wVirtualKeyCode = 0;
    proto.Event.KeyEvent.wVirtualScanCode = 0;
    // Fill these in for each char
    proto.Event.KeyEvent.bKeyDown = TRUE;
    proto.Event.KeyEvent.uChar.UnicodeChar = UNICODE_NULL;

    Log::Comment(NoThrowString().Format(
        L"We're sending utf-16 characters here, because the VtInputThread has "
        L"already converted the ut8 input to utf16 by the time it calls the state machine."));

    // "Л", UTF-16: 0x041B, utf8: "\xd09b"
    std::wstring utf8Input = L"\x041B";
    INPUT_RECORD test = proto;
    test.Event.KeyEvent.uChar.UnicodeChar = utf8Input[0];

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", utf8Input.c_str()));

    testState.vExpectedInput.clear();
    testState.vExpectedInput.push_back(test);
    test.Event.KeyEvent.bKeyDown = FALSE;
    testState.vExpectedInput.push_back(test);
    _stateMachine->ProcessString(utf8Input);

    // "旅", UTF-16: 0x65C5, utf8: "0xE6 0x97 0x85"
    utf8Input = L"\u65C5";
    test = proto;
    test.Event.KeyEvent.uChar.UnicodeChar = utf8Input[0];

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", utf8Input.c_str()));

    testState.vExpectedInput.clear();
    testState.vExpectedInput.push_back(test);
    test.Event.KeyEvent.bKeyDown = FALSE;
    testState.vExpectedInput.push_back(test);
    _stateMachine->ProcessString(utf8Input);
}

void InputEngineTest::CursorPositioningTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    VERIFY_IS_NOT_NULL(dispatch.get());
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch), true);
    VERIFY_IS_NOT_NULL(inputEngine.get());
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(NoThrowString().Format(
        L"Try sending a cursor position response, then send it again. "
        L"The first time, it should be interpreted as a cursor position. "
        L"The state machine engine should reset itself to normal operation "
        L"after that, and treat the second as an F3."));

    std::wstring seq = L"\x1b[1;4R";
    testState._expectCursorPosition = true;
    testState._expectedCursor = { 4, 1 };

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);

    testState._expectCursorPosition = false;

    INPUT_RECORD inputRec;
    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | SHIFT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_F3;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKey(VK_F3, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\0';

    testState.vExpectedInput.push_back(inputRec);
    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);
}

void InputEngineTest::CSICursorBackTabTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = SHIFT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_TAB;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(VK_TAB, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\t';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b[Z";
    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);
}

void InputEngineTest::AltBackspaceTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_BACK;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x08';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b\x7f";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x7f\""));
    _stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltCtrlDTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = 0x44; // D key
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(0x44, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x04';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b\x04";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x04\""));
    _stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltIntermediateTest()
{
    // Tests GH#1209. When we process a alt+key combination where the key just
    // so happens to be an intermediate character, we should make sure that an
    // immediately subsequent ctrl character is handled correctly.

    // We'll test this by creating both a TerminalInput and an
    // InputStateMachine, and piping the KeyEvents generated by the
    // InputStateMachine into the TerminalInput.
    std::wstring expectedTranslation{};

    // First create the callback TerminalInput will call - this will be
    // triggered second, after both the state machine and the TerminalInput have
    // translated the characters.
    auto pfnTerminalInputCallback = [&](std::deque<std::unique_ptr<IInputEvent>>& inEvents) {
        // Get all the characters:
        std::wstring wstr = L"";
        for (auto& ev : inEvents)
        {
            if (ev->EventType() == InputEventType::KeyEvent)
            {
                auto& k = static_cast<KeyEvent&>(*ev);
                auto wch = k.GetCharData();
                wstr += wch;
            }
        }

        VERIFY_ARE_EQUAL(expectedTranslation, wstr);
    };
    TerminalInput terminalInput{ pfnTerminalInputCallback };

    // Create the callback that's fired when the state machine wants to write
    // input. We'll take the events and put them straight into the
    // TerminalInput.
    auto pfnInputStateMachineCallback = [&](std::deque<std::unique_ptr<IInputEvent>>& inEvents) {
        for (auto& ev : inEvents)
        {
            terminalInput.HandleKey(ev.get());
        }
    };
    auto dispatch = std::make_unique<TestInteractDispatch>(pfnInputStateMachineCallback, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(stateMachine);
    testState._stateMachine = stateMachine.get();

    // Write a Alt+/, Ctrl+e pair to the input engine, then take it's output and
    // run it through the terminalInput translator. We should get ^[/^E back
    // out.
    std::wstring seq = L"\x1b/";
    expectedTranslation = seq;
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b/\""));
    stateMachine->ProcessString(seq);

    seq = L"\x05"; // 0x05 is ^E
    expectedTranslation = seq;
    Log::Comment(NoThrowString().Format(L"Processing \"\\x05\""));
    stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltBackspaceEnterTest()
{
    // Created as a test for microsoft/terminal#2746. See that issue for mode
    // details. We're going to send an Alt+Backspace to conpty, followed by an
    // enter. The enter should be processed as just a single VK_ENTER, not a
    // alt+enter.

    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_BACK;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x08';

    // First, expect a alt+backspace.
    testState.vExpectedInput.push_back(inputRec);

    std::wstring seq = L"\x1b\x7f";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x7f\""));
    _stateMachine->ProcessString(seq);

    // Ensure the state machine has correctly returned to the ground state
    VERIFY_ARE_EQUAL(StateMachine::VTStates::Ground, _stateMachine->_state);

    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    inputRec.Event.KeyEvent.dwControlKeyState = 0;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x0d'; //maybe \xa

    // Then, expect a enter
    testState.vExpectedInput.push_back(inputRec);

    seq = L"\x0d";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x0d\""));
    _stateMachine->ProcessString(seq);

    // Ensure the state machine has correctly returned to the ground state
    VERIFY_ARE_EQUAL(StateMachine::VTStates::Ground, _stateMachine->_state);

    VerifyExpectedInputDrained();
}

// Method Description:
// - Writes an SGR VT sequence based on the necessary parameters
// Arguments:
// - button - the state of the buttons (constructed via InputStateMachineEngine::CsiActionMouseCodes)
// - modifiers - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
// - position - the {x,y} position of the event on the viewport where the top-left is {1,1}
// - direction - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)
// Return Value:
// - the SGR VT sequence
std::wstring InputEngineTest::GenerateSgrMouseSequence(const CsiMouseButtonCodes button,
                                                       const unsigned short modifiers,
                                                       const COORD position,
                                                       const CsiActionCodes direction)
{
    // we first need to convert "button" and "modifiers" into an 8 bit sequence
    unsigned int actionCode = 0;

    // button represents the top 2 and bottom 2 bits
    actionCode |= (button & 0b1100);
    actionCode = actionCode << 4;
    actionCode |= (button & 0b0011);

    // modifiers represents the middle 4 bits
    actionCode |= modifiers;

    return wil::str_printf_failfast<std::wstring>(L"\x1b[<%d;%d;%d%c", static_cast<int>(actionCode), position.X, position.Y, direction);
}

void InputEngineTest::VerifySGRMouseData(const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData)
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    SGR_PARAMS input;
    MOUSE_EVENT_PARAMS expected;
    INPUT_RECORD inputRec;
    for (size_t i = 0; i < testData.size(); i++)
    {
        // construct test input
        input = std::get<0>(testData[i]);
        const std::wstring seq = GenerateSgrMouseSequence(std::get<0>(input), std::get<1>(input), std::get<2>(input), std::get<3>(input));

        // construct expected result
        expected = std::get<1>(testData[i]);
        inputRec.EventType = MOUSE_EVENT;
        inputRec.Event.MouseEvent.dwButtonState = std::get<0>(expected);
        inputRec.Event.MouseEvent.dwControlKeyState = std::get<1>(expected);
        inputRec.Event.MouseEvent.dwMousePosition = std::get<2>(expected);
        inputRec.Event.MouseEvent.dwEventFlags = std::get<3>(expected);

        testState.vExpectedInput.push_back(inputRec);

        Log::Comment(NoThrowString().Format(L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }

    VerifyExpectedInputDrained();
}

void InputEngineTest::SGRMouseTest_ButtonClick()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                     EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, 0, { 0, 0 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Modifiers()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                                               EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Shift, { 1, 1 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED, SHIFT_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Shift, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, SHIFT_PRESSED, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, CsiMouseModifierCodes::Meta, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, LEFT_ALT_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, CsiMouseModifierCodes::Meta, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, LEFT_ALT_PRESSED, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Ctrl, { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, LEFT_CTRL_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Ctrl, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, LEFT_CTRL_PRESSED, { 0, 0 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Movement()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                                               EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Right, 0,                           { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Drag, { 1, 2 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 1 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Drag, { 2, 2 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 1, 1 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 2, 2 }, CsiActionCodes::MouseUp },          { 0, 0, { 1, 1 }, 0 } },

        {   { CsiMouseButtonCodes::Left,  0,                           { 2, 2 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 1, 1 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 2, 2 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 1, 1 }, 0 } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Drag,  { 2, 3 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 1, 2 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Drag,  { 3, 3 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 2, 2 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Left, 0,                            { 3, 3 }, CsiActionCodes::MouseUp },          { RIGHTMOST_BUTTON_PRESSED, 0, { 2, 2 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 3, 3 }, CsiActionCodes::MouseUp },          { 0, 0, { 2, 2 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Scroll()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    // NOTE: scrolling events do NOT send a mouse up event
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                             EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::ScrollForward, 0, { 1, 1 }, CsiActionCodes::MouseDown },        { SCROLL_DELTA_FORWARD,  0, { 0, 0 }, MOUSE_WHEELED } },
        {   { CsiMouseButtonCodes::ScrollBack,    0, { 1, 1 }, CsiActionCodes::MouseDown },        { SCROLL_DELTA_BACKWARD, 0, { 0, 0 }, MOUSE_WHEELED } },
    };
    // clang-format on
    VerifySGRMouseData(testData);
}
