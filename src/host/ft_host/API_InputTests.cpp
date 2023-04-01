// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <thread>

#include "../../interactivity/onecore/SystemConfigurationProvider.hpp"

// some assumptions have been made on this value. only change it if you have a good reason to.
#define NUMBER_OF_SCENARIO_INPUTS 10
#define READ_BATCH 3

using namespace WEX::Logging;
using namespace WEX::Common;

// This class is intended to test:
// FlushConsoleInputBuffer
// PeekConsoleInput
// ReadConsoleInput
// WriteConsoleInput
// GetNumberOfConsoleInputEvents
// GetNumberOfConsoleMouseButtons
class InputTests
{
    BEGIN_TEST_CLASS(InputTests)
    END_TEST_CLASS()

    TEST_CLASS_SETUP(TestSetup);
    TEST_CLASS_CLEANUP(TestCleanup);

    // note: GetNumberOfConsoleMouseButtons crashes with nullptr, so there's no negative test
    TEST_METHOD(TestGetMouseButtonsValid);

    TEST_METHOD(TestInputScenario);
    TEST_METHOD(TestFlushValid);
    TEST_METHOD(TestFlushInvalid);
    TEST_METHOD(TestPeekConsoleInvalid);
    TEST_METHOD(TestReadConsoleInvalid);
    TEST_METHOD(TestWriteConsoleInvalid);

    TEST_METHOD(TestReadWaitOnHandle);

    TEST_METHOD(TestReadConsolePasswordScenario);

    TEST_METHOD(TestMouseWheelReadConsoleMouseInput);
    TEST_METHOD(TestMouseHorizWheelReadConsoleMouseInput);
    TEST_METHOD(TestMouseWheelReadConsoleNoMouseInput);
    TEST_METHOD(TestMouseHorizWheelReadConsoleNoMouseInput);
    TEST_METHOD(TestMouseWheelReadConsoleInputQuickEdit);
    TEST_METHOD(TestMouseHorizWheelReadConsoleInputQuickEdit);
    TEST_METHOD(RawReadUnpacksCoalescedInputRecords);

    BEGIN_TEST_METHOD(TestVtInputGeneration)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD();
};

void VerifyNumberOfInputRecords(const HANDLE hConsoleInput, _In_ DWORD nInputs)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    auto nInputEvents = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &nInputEvents));
    VERIFY_ARE_EQUAL(nInputEvents,
                     nInputs,
                     L"Verify number of input events");
}

bool InputTests::TestSetup()
{
    const auto fRet = Common::TestBufferSetup();

    auto hConsoleInput = GetStdInputHandle();
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
    VerifyNumberOfInputRecords(hConsoleInput, 0);

    return fRet;
}

bool InputTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void InputTests::TestGetMouseButtonsValid()
{
    auto nMouseButtons = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(OneCoreDelay::GetNumberOfConsoleMouseButtons(&nMouseButtons));

    auto dwButtonsExpected = (DWORD)-1;
    if (OneCoreDelay::IsGetSystemMetricsPresent())
    {
        dwButtonsExpected = (DWORD)GetSystemMetrics(SM_CMOUSEBUTTONS);
    }
    else
    {
        dwButtonsExpected = Microsoft::Console::Interactivity::OneCore::SystemConfigurationProvider::s_DefaultNumberOfMouseButtons;
    }

    VERIFY_ARE_EQUAL(dwButtonsExpected, nMouseButtons);
}

void GenerateAndWriteInputRecords(const HANDLE hConsoleInput,
                                  const UINT cRecordsToGenerate,
                                  _Out_writes_(cRecs) INPUT_RECORD* prgRecs,
                                  const DWORD cRecs,
                                  _Out_ PDWORD pdwWritten)
{
    Log::Comment(String().Format(L"Generating %d input events", cRecordsToGenerate));
    for (UINT iRecord = 0; iRecord < cRecs; iRecord++)
    {
        prgRecs[iRecord].EventType = KEY_EVENT;
        prgRecs[iRecord].Event.KeyEvent.bKeyDown = FALSE;
        prgRecs[iRecord].Event.KeyEvent.wRepeatCount = 1;
        prgRecs[iRecord].Event.KeyEvent.wVirtualKeyCode = ('A' + (WORD)iRecord);
    }

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, prgRecs, cRecs, pdwWritten));
    VERIFY_ARE_EQUAL(*pdwWritten,
                     cRecs,
                     L"verify number written");
}

void InputTests::TestInputScenario()
{
    Log::Comment(L"Get input handle");
    auto hConsoleInput = GetStdInputHandle();

    auto nWrittenEvents = (DWORD)-1;
    INPUT_RECORD rgInputRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, NUMBER_OF_SCENARIO_INPUTS, rgInputRecords, ARRAYSIZE(rgInputRecords), &nWrittenEvents);

    VerifyNumberOfInputRecords(hConsoleInput, ARRAYSIZE(rgInputRecords));

    Log::Comment(L"Peeking events");
    INPUT_RECORD rgPeekedRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    auto nPeekedEvents = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(PeekConsoleInput(hConsoleInput, rgPeekedRecords, ARRAYSIZE(rgPeekedRecords), &nPeekedEvents));
    VERIFY_ARE_EQUAL(nPeekedEvents, nWrittenEvents, L"We should be able to peek at all of the records we've written");
    for (UINT iPeekedRecord = 0; iPeekedRecord < nPeekedEvents; iPeekedRecord++)
    {
        VERIFY_ARE_EQUAL(rgPeekedRecords[iPeekedRecord],
                         rgInputRecords[iPeekedRecord],
                         L"make sure our peeked records match what we input");
    }

    // read inputs 3 at a time until we've read them all. since the number we're batching by doesn't match the number of
    // total events, we need to account for the last incomplete read we'll perform.
    const UINT cIterations = (NUMBER_OF_SCENARIO_INPUTS / READ_BATCH) + ((NUMBER_OF_SCENARIO_INPUTS % READ_BATCH > 0) ? 1 : 0);
    for (UINT iIteration = 0; iIteration < cIterations; iIteration++)
    {
        const auto fIsLastIteration = (iIteration + 1) > (NUMBER_OF_SCENARIO_INPUTS / READ_BATCH);
        Log::Comment(String().Format(L"Reading inputs (iteration %d/%d)%s",
                                     iIteration + 1,
                                     cIterations,
                                     fIsLastIteration ? L" (last one)" : L""));

        INPUT_RECORD rgReadRecords[READ_BATCH] = { 0 };
        auto nReadEvents = (DWORD)-1;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, rgReadRecords, ARRAYSIZE(rgReadRecords), &nReadEvents));

        DWORD dwExpectedEventsRead = READ_BATCH;
        if (fIsLastIteration)
        {
            // on the last iteration, we'll have an incomplete read. account for it here.
            dwExpectedEventsRead = NUMBER_OF_SCENARIO_INPUTS % READ_BATCH;
        }

        VERIFY_ARE_EQUAL(nReadEvents, dwExpectedEventsRead);
        for (UINT iReadRecord = 0; iReadRecord < nReadEvents; iReadRecord++)
        {
            const auto iInputRecord = iReadRecord + (iIteration * READ_BATCH);
            VERIFY_ARE_EQUAL(rgReadRecords[iReadRecord],
                             rgInputRecords[iInputRecord],
                             String().Format(L"verify record %d", iInputRecord));
        }

        auto nInputEventsAfterRead = (DWORD)-1;
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &nInputEventsAfterRead));

        DWORD dwExpectedEventsAfterRead = (NUMBER_OF_SCENARIO_INPUTS - (READ_BATCH * (iIteration + 1)));
        if (fIsLastIteration)
        {
            dwExpectedEventsAfterRead = 0;
        }
        VERIFY_ARE_EQUAL(dwExpectedEventsAfterRead,
                         nInputEventsAfterRead,
                         L"verify number of remaining inputs");
    }
}

void InputTests::TestFlushValid()
{
    Log::Comment(L"Get input handle");
    auto hConsoleInput = GetStdInputHandle();

    auto nWrittenEvents = (DWORD)-1;
    INPUT_RECORD rgInputRecords[NUMBER_OF_SCENARIO_INPUTS] = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, NUMBER_OF_SCENARIO_INPUTS, rgInputRecords, ARRAYSIZE(rgInputRecords), &nWrittenEvents);

    VerifyNumberOfInputRecords(hConsoleInput, ARRAYSIZE(rgInputRecords));

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));

    VerifyNumberOfInputRecords(hConsoleInput, 0);
}

void InputTests::TestFlushInvalid()
{
    // NOTE: FlushConsoleInputBuffer(nullptr) crashes, so we don't verify that here.
    VERIFY_WIN32_BOOL_FAILED(FlushConsoleInputBuffer(INVALID_HANDLE_VALUE));
}

void InputTests::TestPeekConsoleInvalid()
{
    auto nPeeked = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(PeekConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nPeeked)); // NOTE: nPeeked is required
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0);

    auto hConsoleInput = GetStdInputHandle();

    nPeeked = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(PeekConsoleInput(hConsoleInput, nullptr, 5, &nPeeked));
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0);

    auto nWritten = (DWORD)-1;
    INPUT_RECORD ir = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, 1, &ir, 1, &nWritten);

    VerifyNumberOfInputRecords(hConsoleInput, 1);

    nPeeked = (DWORD)-1;
    INPUT_RECORD irPeeked = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(PeekConsoleInput(hConsoleInput, &irPeeked, 0, &nPeeked));
    VERIFY_ARE_EQUAL(nPeeked, (DWORD)0, L"Verify that an empty array doesn't cause peeks to get written");

    VerifyNumberOfInputRecords(hConsoleInput, 1);

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
}

void InputTests::TestReadConsoleInvalid()
{
    auto nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(ReadConsoleInput(nullptr, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(ReadConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    // NOTE: ReadConsoleInput blocks until at least one input event is read, even if the operation would result in no
    // records actually being read (e.g. valid handle, NULL lpBuffer)

    auto hConsoleInput = GetStdInputHandle();

    auto nWritten = (DWORD)-1;
    INPUT_RECORD irWrite = { 0 };
    GenerateAndWriteInputRecords(hConsoleInput, 1, &irWrite, 1, &nWritten);
    VerifyNumberOfInputRecords(hConsoleInput, 1);

    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, nullptr, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    INPUT_RECORD irRead = { 0 };
    nRead = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hConsoleInput, &irRead, 0, &nRead));
    VERIFY_ARE_EQUAL(nRead, (DWORD)0);

    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput));
}

void InputTests::TestWriteConsoleInvalid()
{
    auto nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(WriteConsoleInput(nullptr, nullptr, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);

    // weird: WriteConsoleInput with INVALID_HANDLE_VALUE writes garbage to lpNumberOfEventsWritten, whereas
    // [Read|Peek]ConsoleInput don't. This is a legacy behavior that we don't want to change.
    nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_FAILED(WriteConsoleInput(INVALID_HANDLE_VALUE, nullptr, 0, &nWrite));

    auto hConsoleInput = GetStdInputHandle();

    nWrite = (DWORD)-1;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, nullptr, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);

    nWrite = (DWORD)-1;
    INPUT_RECORD irWrite = { 0 };
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hConsoleInput, &irWrite, 0, &nWrite));
    VERIFY_ARE_EQUAL(nWrite, (DWORD)0);
}

void FillInputRecordHelper(_Inout_ INPUT_RECORD* const pir, _In_ wchar_t wch, _In_ bool fIsKeyDown)
{
    pir->EventType = KEY_EVENT;
    pir->Event.KeyEvent.wRepeatCount = 1;
    pir->Event.KeyEvent.dwControlKeyState = 0;
    pir->Event.KeyEvent.bKeyDown = !!fIsKeyDown;
    pir->Event.KeyEvent.uChar.UnicodeChar = wch;

    // This only holds true for capital letters from A-Z.
    VERIFY_IS_TRUE(wch >= 'A' && wch <= 'Z');
    pir->Event.KeyEvent.wVirtualKeyCode = wch;

    pir->Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(pir->Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC));
}

void InputTests::TestReadConsolePasswordScenario()
{
    if (!OneCoreDelay::IsPostMessageWPresent())
    {
        Log::Comment(L"Password scenario can't be checked on platform without window message queuing.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    // Scenario inspired by net use's password capture code.
    const auto hIn = GetStdHandle(STD_INPUT_HANDLE);

    // 1. Set up our mode to be raw input (mimicking method used by "net use")
    DWORD mode = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT;
    GetConsoleMode(hIn, &mode);

    SetConsoleMode(hIn, (~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)) & mode);

    // 2. Flush and write some text into the input buffer (added for sake of test.)
    auto pwszExpected = L"QUE";
    const auto cBuffer = static_cast<DWORD>(wcslen(pwszExpected) * 2);
    auto irBuffer = wil::make_unique_nothrow<INPUT_RECORD[]>(cBuffer);
    FillInputRecordHelper(&irBuffer.get()[0], pwszExpected[0], true);
    FillInputRecordHelper(&irBuffer.get()[1], pwszExpected[0], false);
    FillInputRecordHelper(&irBuffer.get()[2], pwszExpected[1], true);
    FillInputRecordHelper(&irBuffer.get()[3], pwszExpected[1], false);
    FillInputRecordHelper(&irBuffer.get()[4], pwszExpected[2], true);
    FillInputRecordHelper(&irBuffer.get()[5], pwszExpected[2], false);

    DWORD dwWritten;
    FlushConsoleInputBuffer(hIn);
    WriteConsoleInputW(hIn, irBuffer.get(), cBuffer, &dwWritten);

    // Press "enter" key on the window to signify the user pressing enter at the end of the password.

    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(PostMessageW(GetConsoleWindow(), WM_KEYDOWN, VK_RETURN, 0));

    // 3. Set up our read loop (mimicking password capture methodology from "net use" command.)
    const size_t buflen = (cBuffer / 2) + 1; // key down and key up will be coalesced into one.
    auto buf = wil::make_unique_nothrow<wchar_t[]>(buflen);
    size_t len = 0;
    VERIFY_IS_NOT_NULL(buf);
    auto bufPtr = buf.get();

    while (true)
    {
        wchar_t ch;
        DWORD c;
        auto err = ReadConsoleW(hIn, &ch, 1, &c, nullptr);

        if (!err || c != 1)
        {
            ch = 0xffff; // end of line
        }

        if ((ch == 0xD) || (ch == 0xffff)) // CR or end of line
        {
            break;
        }

        if (ch == 0x8) // backspace
        {
            if (bufPtr != buf.get())
            {
                bufPtr--;
                len--;
            }
        }
        else
        {
            *bufPtr = ch;
            if (len < buflen)
            {
                bufPtr++;
            }
            len++;
        }
    }

    // 4. Restore console mode and terminate string (mimics "net use" behavior)
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);

    *bufPtr = L'\0'; // null terminate string
    putchar('\n');

    // 5. Verify our string got read back (added for sake of the test)
    VERIFY_ARE_EQUAL(String(pwszExpected), String(buf.get()));
    VERIFY_ARE_EQUAL(wcslen(pwszExpected), len);
}

void TestMouseWheelReadConsoleInputHelper(const UINT /*msg*/, const DWORD /*dwEventFlagsExpected*/, const DWORD /*dwConsoleMode*/)
{
    if (!OneCoreDelay::IsIsWindowPresent())
    {
        Log::Comment(L"Mouse wheel with respect to a window can't be checked on platform without classic window message queuing.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    Log::Comment(L"This test is flaky. Fix me in GH#4494");
    Log::Result(WEX::Logging::TestResults::Skipped);
    return;

    //const auto hwnd = GetConsoleWindow();
    //VERIFY_IS_TRUE(!!IsWindow(hwnd), L"Get console window handle to inject wheel messages.");

    //const auto hConsoleInput = GetStdInputHandle();
    //VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hConsoleInput, dwConsoleMode), L"Apply the requested console mode");

    //// We don't generate mouse console event in QuickEditMode or if MouseInput is not enabled
    //DWORD dwExpectedEvents = 1;
    //if (dwConsoleMode & ENABLE_QUICK_EDIT_MODE || !(dwConsoleMode & ENABLE_MOUSE_INPUT))
    //{
    //    Log::Comment(L"QuickEditMode is set or MouseInput is not set, not expecting events");
    //    dwExpectedEvents = 0;
    //}

    //VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hConsoleInput), L"Flush input queue to make sure no one else is in the way.");

    //// WM_MOUSEWHEEL params
    //// https://msdn.microsoft.com/en-us/library/windows/desktop/ms645617(v=vs.85).aspx

    //// WPARAM is HIWORD the wheel delta and LOWORD the keystate (keys pressed with it)
    //// We want no keys pressed in the loword (0) and we want one tick of the wheel in the high word.
    //WPARAM wParam = 0;
    //short sKeyState = 0;
    //short sWheelDelta = -WHEEL_DELTA; // scroll down is negative, up is positive.
    //// we only use the lower 32-bits (in case of 64-bit system)
    //wParam = ((sWheelDelta << 16) | sKeyState) & 0xFFFFFFFF;

    //// LPARAM is positioning information. We don't care so we'll leave it 0x0
    //LPARAM lParam = 0;

    //Log::Comment(L"Send scroll down message into console window queue.");
    //SendMessageW(hwnd, msg, wParam, lParam);

    //Sleep(250); // give message time to sink in

    //DWORD dwAvailable = 0;
    //VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hConsoleInput, &dwAvailable), L"Retrieve number of events in queue.");
    //VERIFY_ARE_EQUAL(dwExpectedEvents, dwAvailable, NoThrowString().Format(L"We expected %i event from our scroll message.", dwExpectedEvents));

    //INPUT_RECORD ir;
    //DWORD dwRead = 0;
    //if (dwExpectedEvents == 1)
    //{
    //    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInputW(hConsoleInput, &ir, 1, &dwRead), L"Read the event out.");
    //    VERIFY_ARE_EQUAL(1u, dwRead);

    //    Log::Comment(L"Verify the event is what we expected. We only verify the fields relevant to this test.");
    //    VERIFY_ARE_EQUAL(MOUSE_EVENT, ir.EventType);
    //    // hard cast OK. only using lower 32-bits (see above)
    //    VERIFY_ARE_EQUAL((DWORD)wParam, ir.Event.MouseEvent.dwButtonState);
    //    // Don't care about ctrl key state. Can be messed with by caps lock/numlock state. Not checking this.
    //    VERIFY_ARE_EQUAL(dwEventFlagsExpected, ir.Event.MouseEvent.dwEventFlags);
    //    // Don't care about mouse position for ensuring scroll message went through.
    //}
}

void InputTests::TestMouseWheelReadConsoleMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestMouseWheelReadConsoleNoMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleNoMouseInput()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_EXTENDED_FLAGS;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestMouseWheelReadConsoleInputQuickEdit()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEWHEEL, MOUSE_WHEELED, dwInputMode);
}

void InputTests::TestMouseHorizWheelReadConsoleInputQuickEdit()
{
    const DWORD dwInputMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE;
    TestMouseWheelReadConsoleInputHelper(WM_MOUSEHWHEEL, MOUSE_HWHEELED, dwInputMode);
}

void InputTests::TestReadWaitOnHandle()
{
    const auto hIn = GetStdInputHandle();
    VERIFY_IS_NOT_NULL(hIn, L"Check input handle is not null.");

    // Set up events and background thread to wait.
    auto fAbortWait = false;

    // this will be signaled when we want the thread to start waiting on the input handle
    // It is an auto-reset.
    wil::unique_event doWait;
    doWait.create();

    // the thread will signal this when it is done waiting on the input handle.
    // It is an auto-reset.
    wil::unique_event doneWaiting;
    doneWaiting.create();

    std::thread bgThread([&] {
        while (!fAbortWait)
        {
            doWait.wait();

            if (fAbortWait)
            {
                break;
            }

            HANDLE waits[2];
            waits[0] = doWait.get();
            waits[1] = hIn;
            WaitForMultipleObjects(2, waits, FALSE, INFINITE);

            if (fAbortWait)
            {
                break;
            }

            doneWaiting.SetEvent();
        }
    });

    auto onExit = wil::scope_exit([&] {
        Log::Comment(L"Tell our background thread to abort waiting, signal it, then wait for it to exit before we finish the test.");
        fAbortWait = true;
        doWait.SetEvent();
        bgThread.join();
    });

    Log::Comment(L"Test 1: Waiting for text to be appended to the buffer.");
    // Empty the buffer and tell the thread to start waiting
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Ensure input buffer is empty.");
    doWait.SetEvent();

    // Send some input into the console.
    INPUT_RECORD ir;
    ir.EventType = MOUSE_EVENT;
    ir.Event.MouseEvent.dwMousePosition.X = 1;
    ir.Event.MouseEvent.dwMousePosition.Y = 1;
    ir.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
    ir.Event.MouseEvent.dwControlKeyState = NUMLOCK_ON;
    ir.Event.MouseEvent.dwEventFlags = 0;

    DWORD dwWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(hIn, &ir, 1, &dwWritten), L"Inject input event into queue.");
    VERIFY_ARE_EQUAL(1u, dwWritten, L"Ensure 1 event was written.");

    VERIFY_IS_TRUE(doneWaiting.wait(5000), L"The input handle should have been signaled on our background thread within our 5 second timeout.");

    Log::Comment(L"Test 2: Trigger a VT response so the buffer will be prepended (things inserted at the front).");

    const auto hOut = GetStdOutputHandle();
    DWORD dwMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(hOut, &dwMode), L"Get existing console mode.");
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING), L"Ensure VT mode is on.");

    // Empty the buffer and tell the thread to start waiting
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn), L"Ensure input buffer is empty.");
    doWait.SetEvent();

    // Send a VT command that will trigger a response.
    auto pwszDeviceAttributeRequest = L"\x1b[c";
    auto cchDeviceAttributeRequest = static_cast<DWORD>(wcslen(pwszDeviceAttributeRequest));
    dwWritten = 0;
    WriteConsoleW(hOut, pwszDeviceAttributeRequest, cchDeviceAttributeRequest, &dwWritten, nullptr);
    VERIFY_ARE_EQUAL(cchDeviceAttributeRequest, dwWritten, L"Verify string was written");

    VERIFY_IS_TRUE(doneWaiting.wait(5000), L"The input handle should have been signaled on our background thread within our 5 second timeout.");
}

void InputTests::TestVtInputGeneration()
{
    Log::Comment(L"Get input handle");
    auto hIn = GetStdInputHandle();

    DWORD dwMode;
    GetConsoleMode(hIn, &dwMode);

    auto dwWritten = (DWORD)-1;
    auto dwRead = (DWORD)-1;
    INPUT_RECORD rgInputRecords[64] = { 0 };

    Log::Comment(L"First make sure that an arrow keydown is not translated in not-VT mode");

    dwMode = WI_ClearFlag(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(hIn, dwMode);
    GetConsoleMode(hIn, &dwMode);
    VERIFY_IS_FALSE(WI_IsFlagSet(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT));

    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = VK_UP;

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, rgInputRecords, 1, &dwWritten));
    VERIFY_ARE_EQUAL(dwWritten, (DWORD)1);

    Log::Comment(L"Reading events");
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hIn, rgInputRecords, ARRAYSIZE(rgInputRecords), &dwRead));
    VERIFY_ARE_EQUAL(dwRead, (DWORD)1);
    VERIFY_ARE_EQUAL(rgInputRecords[0].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode, VK_UP);

    Log::Comment(L"Now, enable VT Input and make sure that a vt sequence comes out the other side.");

    dwMode = WI_SetFlag(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(hIn, dwMode);
    GetConsoleMode(hIn, &dwMode);
    VERIFY_IS_TRUE(WI_IsFlagSet(dwMode, ENABLE_VIRTUAL_TERMINAL_INPUT));

    Log::Comment(L"Flushing");
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn));

    rgInputRecords[0].EventType = KEY_EVENT;
    rgInputRecords[0].Event.KeyEvent.bKeyDown = TRUE;
    rgInputRecords[0].Event.KeyEvent.wRepeatCount = 1;
    rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode = VK_UP;

    Log::Comment(L"Writing events");
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, rgInputRecords, 1, &dwWritten));
    VERIFY_ARE_EQUAL(dwWritten, (DWORD)1);

    Log::Comment(L"Reading events");
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInput(hIn, rgInputRecords, ARRAYSIZE(rgInputRecords), &dwRead));
    VERIFY_ARE_EQUAL(dwRead, (DWORD)3);
    VERIFY_ARE_EQUAL(rgInputRecords[0].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[0].Event.KeyEvent.uChar.UnicodeChar, L'\x1b');

    VERIFY_ARE_EQUAL(rgInputRecords[1].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[1].Event.KeyEvent.uChar.UnicodeChar, L'[');

    VERIFY_ARE_EQUAL(rgInputRecords[2].EventType, KEY_EVENT);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.bKeyDown, TRUE);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.wVirtualKeyCode, 0);
    VERIFY_ARE_EQUAL(rgInputRecords[2].Event.KeyEvent.uChar.UnicodeChar, L'A');
}

void InputTests::RawReadUnpacksCoalescedInputRecords()
{
    DWORD mode = 0;
    auto hIn = GetStdInputHandle();
    const auto writeWch = L'a';
    const auto repeatCount = 5;

    // turn on raw mode
    GetConsoleMode(hIn, &mode);
    WI_ClearFlag(mode, ENABLE_LINE_INPUT);
    SetConsoleMode(hIn, mode);

    // flush input queue before attempting to add new events and check
    // in case any are leftover from previous tests
    VERIFY_WIN32_BOOL_SUCCEEDED(FlushConsoleInputBuffer(hIn));

    INPUT_RECORD record;
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wRepeatCount = repeatCount;
    record.Event.KeyEvent.wVirtualKeyCode = writeWch;
    record.Event.KeyEvent.uChar.UnicodeChar = writeWch;

    // write an event with a repeat count
    DWORD writtenAmount = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(hIn, &record, 1, &writtenAmount));
    VERIFY_ARE_EQUAL(writtenAmount, static_cast<DWORD>(1));

    // stream read the events out one at a time
    DWORD eventCount = 0;
    for (auto i = 0; i < repeatCount; ++i)
    {
        eventCount = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hIn, &eventCount));
        VERIFY_IS_TRUE(eventCount > 0);

        wchar_t wch;
        DWORD readAmount;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsole(hIn, &wch, 1, &readAmount, NULL));
        VERIFY_ARE_EQUAL(readAmount, static_cast<DWORD>(1));
        VERIFY_ARE_EQUAL(wch, writeWch);
    }

    // input buffer should now be empty
    eventCount = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetNumberOfConsoleInputEvents(hIn, &eventCount));
    VERIFY_ARE_EQUAL(eventCount, static_cast<DWORD>(0));
}
