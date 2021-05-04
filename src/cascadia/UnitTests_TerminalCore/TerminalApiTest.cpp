// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
#define WCS(x) WCSHELPER(x)
#define WCSHELPER(x) L#x

    class TerminalApiTest
    {
        TEST_CLASS(TerminalApiTest);

        TEST_METHOD(SetColorTableEntry);

        TEST_METHOD(CursorVisibility);
        TEST_METHOD(CursorVisibilityViaStateMachine);

        // Terminal::_WriteBuffer used to enter infinite loops under certain conditions.
        // This test ensures that Terminal::_WriteBuffer doesn't get stuck when
        // PrintString() is called with more code units than the buffer width.
        TEST_METHOD(PrintStringOfSurrogatePairs);
        TEST_METHOD(CheckDoubleWidthCursor);

        TEST_METHOD(AddHyperlink);
        TEST_METHOD(AddHyperlinkCustomId);
        TEST_METHOD(AddHyperlinkCustomIdDifferentUri);

        TEST_METHOD(SetTaskbarProgress);
        TEST_METHOD(SetWorkingDirectory);
    };
};

using namespace TerminalCoreUnitTests;

void TerminalApiTest::SetColorTableEntry()
{
    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto settings = winrt::make<MockTermSettings>(100, 100, 100);
    term.UpdateSettings(settings);

    VERIFY_IS_TRUE(term.SetColorTableEntry(0, 100));
    VERIFY_IS_TRUE(term.SetColorTableEntry(128, 100));
    VERIFY_IS_TRUE(term.SetColorTableEntry(255, 100));

    VERIFY_IS_FALSE(term.SetColorTableEntry(256, 100));
    VERIFY_IS_FALSE(term.SetColorTableEntry(512, 100));
}

// Terminal::_WriteBuffer used to enter infinite loops under certain conditions.
// This test ensures that Terminal::_WriteBuffer doesn't get stuck when
// PrintString() is called with more code units than the buffer width.
void TerminalApiTest::PrintStringOfSurrogatePairs()
{
    DummyRenderTarget renderTarget;
    Terminal term;
    term.Create({ 100, 100 }, 3, renderTarget);

    std::wstring text;
    text.reserve(600);

    for (size_t i = 0; i < 100; ++i)
    {
        text.append(L"𐐌𐐜𐐬");
    }

    struct Baton
    {
        HANDLE done;
        std::wstring text;
        Terminal* pTerm;
    } baton{
        CreateEventW(nullptr, TRUE, FALSE, L"done signal"),
        text,
        &term,
    };

    Log::Comment(L"Launching thread to write data.");
    const auto thread = CreateThread(
        nullptr,
        0,
        [](LPVOID data) -> DWORD {
            const Baton& baton = *reinterpret_cast<Baton*>(data);
            Log::Comment(L"Writing data.");
            baton.pTerm->PrintString(baton.text);
            Log::Comment(L"Setting event.");
            SetEvent(baton.done);
            return 0;
        },
        (LPVOID)&baton,
        0,
        nullptr);

    Log::Comment(L"Waiting for the write.");
    switch (WaitForSingleObject(baton.done, 2000))
    {
    case WAIT_OBJECT_0:
        Log::Comment(L"Didn't get stuck. Success.");
        break;
    case WAIT_TIMEOUT:
        Log::Comment(L"Wait timed out. It got stuck.");
        Log::Result(WEX::Logging::TestResults::Failed);
        break;
    case WAIT_FAILED:
        Log::Comment(L"Wait failed for some reason. We didn't expect this.");
        Log::Result(WEX::Logging::TestResults::Failed);
        break;
    default:
        Log::Comment(L"Wait return code that no one expected. Fail.");
        Log::Result(WEX::Logging::TestResults::Failed);
        break;
    }

    TerminateThread(thread, 0);
    CloseHandle(baton.done);
    return;
}

void TerminalApiTest::CursorVisibility()
{
    // GH#3093 - Cursor Visibility and On states shouldn't affect each other
    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    VERIFY_IS_TRUE(term._buffer->GetCursor().IsVisible());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsBlinkingAllowed());

    term.SetCursorOn(false);
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsVisible());
    VERIFY_IS_FALSE(term._buffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsBlinkingAllowed());

    term.SetCursorOn(true);
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsVisible());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsBlinkingAllowed());

    term.SetCursorVisibility(false);
    VERIFY_IS_FALSE(term._buffer->GetCursor().IsVisible());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsBlinkingAllowed());

    term.SetCursorOn(false);
    VERIFY_IS_FALSE(term._buffer->GetCursor().IsVisible());
    VERIFY_IS_FALSE(term._buffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(term._buffer->GetCursor().IsBlinkingAllowed());
}

void TerminalApiTest::CursorVisibilityViaStateMachine()
{
    // This is a nearly literal copy-paste of ScreenBufferTests::TestCursorIsOn, adapted for the Terminal
    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& tbi = *(term._buffer);
    auto& stateMachine = *(term._stateMachine);
    auto& cursor = tbi.GetCursor();

    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    cursor.SetIsOn(false);
    stateMachine.ProcessString(L"\x1b[?12l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?25l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_FALSE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?25h");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_TRUE(cursor.IsBlinkingAllowed());
    VERIFY_IS_TRUE(cursor.IsVisible());

    stateMachine.ProcessString(L"\x1b[?12;25l");
    VERIFY_IS_TRUE(cursor.IsOn());
    VERIFY_IS_FALSE(cursor.IsBlinkingAllowed());
    VERIFY_IS_FALSE(cursor.IsVisible());
}

void TerminalApiTest::CheckDoubleWidthCursor()
{
    DummyRenderTarget renderTarget;
    Terminal term;
    term.Create({ 100, 100 }, 0, renderTarget);

    auto& tbi = *(term._buffer);
    auto& stateMachine = *(term._stateMachine);
    auto& cursor = tbi.GetCursor();

    // Lets stuff the buffer with single width characters,
    // but leave the last two columns empty for double width.
    std::wstring singleWidthText;
    singleWidthText.reserve(98);
    for (size_t i = 0; i < 98; ++i)
    {
        singleWidthText.append(L"A");
    }
    stateMachine.ProcessString(singleWidthText);
    VERIFY_IS_TRUE(cursor.GetPosition().X == 98);

    // Stuff two double width characters.
    std::wstring doubleWidthText{ L"我愛" };
    stateMachine.ProcessString(doubleWidthText);

    // The last 'A'
    term.SetCursorPosition(97, 0);
    VERIFY_IS_FALSE(term.IsCursorDoubleWidth());

    // This and the next CursorPos are taken up by '我‘
    term.SetCursorPosition(98, 0);
    VERIFY_IS_TRUE(term.IsCursorDoubleWidth());
    term.SetCursorPosition(99, 0);
    VERIFY_IS_TRUE(term.IsCursorDoubleWidth());

    // This and the next CursorPos are taken up by ’愛‘
    term.SetCursorPosition(0, 1);
    VERIFY_IS_TRUE(term.IsCursorDoubleWidth());
    term.SetCursorPosition(1, 1);
    VERIFY_IS_TRUE(term.IsCursorDoubleWidth());
}

void TerminalCoreUnitTests::TerminalApiTest::AddHyperlink()
{
    // This is a nearly literal copy-paste of ScreenBufferTests::TestAddHyperlink, adapted for the Terminal

    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& tbi = *(term._buffer);
    auto& stateMachine = *(term._stateMachine);

    // Process the opening osc 8 sequence
    stateMachine.ProcessString(L"\x1b]8;;test.url\x9c");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");

    // Send any other text
    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");

    // Process the closing osc 8 sequences
    stateMachine.ProcessString(L"\x1b]8;;\x9c");
    VERIFY_IS_FALSE(tbi.GetCurrentAttributes().IsHyperlink());
}

void TerminalCoreUnitTests::TerminalApiTest::AddHyperlinkCustomId()
{
    // This is a nearly literal copy-paste of ScreenBufferTests::TestAddHyperlinkCustomId, adapted for the Terminal

    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& tbi = *(term._buffer);
    auto& stateMachine = *(term._stateMachine);

    // Process the opening osc 8 sequence
    stateMachine.ProcessString(L"\x1b]8;id=myId;test.url\x9c");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // Send any other text
    stateMachine.ProcessString(L"Hello World");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // Process the closing osc 8 sequences
    stateMachine.ProcessString(L"\x1b]8;;\x9c");
    VERIFY_IS_FALSE(tbi.GetCurrentAttributes().IsHyperlink());
}

void TerminalCoreUnitTests::TerminalApiTest::AddHyperlinkCustomIdDifferentUri()
{
    // This is a nearly literal copy-paste of ScreenBufferTests::TestAddHyperlinkCustomId, adapted for the Terminal

    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& tbi = *(term._buffer);
    auto& stateMachine = *(term._stateMachine);

    // Process the opening osc 8 sequence
    stateMachine.ProcessString(L"\x1b]8;id=myId;test.url\x9c");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"test.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"test.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    const auto oldAttributes{ tbi.GetCurrentAttributes() };

    // Send any other text
    stateMachine.ProcessString(L"\x1b]8;id=myId;other.url\x9c");
    VERIFY_IS_TRUE(tbi.GetCurrentAttributes().IsHyperlink());
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(tbi.GetCurrentAttributes().GetHyperlinkId()), L"other.url");
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkId(L"other.url", L"myId"), tbi.GetCurrentAttributes().GetHyperlinkId());

    // This second URL should not change the URL of the original ID!
    VERIFY_ARE_EQUAL(tbi.GetHyperlinkUriFromId(oldAttributes.GetHyperlinkId()), L"test.url");
    VERIFY_ARE_NOT_EQUAL(oldAttributes.GetHyperlinkId(), tbi.GetCurrentAttributes().GetHyperlinkId());
}

void TerminalCoreUnitTests::TerminalApiTest::SetTaskbarProgress()
{
    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& stateMachine = *(term._stateMachine);

    // Initial values for taskbar state and progress should be 0
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(0));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));

    // Set some values for taskbar state and progress through state machine
    stateMachine.ProcessString(L"\x1b]9;4;1;50\x9c");
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(1));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(50));

    // Reset to 0
    stateMachine.ProcessString(L"\x1b]9;4;0;0\x9c");
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(0));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));

    // Set an out of bounds value for state
    stateMachine.ProcessString(L"\x1b]9;4;5;50\x9c");
    // Nothing should have changed (dispatch should have returned false)
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(0));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));

    // Set an out of bounds value for progress
    stateMachine.ProcessString(L"\x1b]9;4;1;999\x9c");
    // Progress should have been clamped to 100
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(1));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(100));

    // Don't specify any params
    stateMachine.ProcessString(L"\x1b]9;4\x9c");
    // State and progress should both be reset to 0
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(0));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));

    // Specify additional params
    stateMachine.ProcessString(L"\x1b]9;4;1;80;123\x9c");
    // Additional params should be ignored, state and progress still set normally
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(1));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(80));

    // Edge cases + trailing semicolon testing
    stateMachine.ProcessString(L"\x1b]9;4;2;\x9c");
    // String should be processed correctly despite the trailing semicolon,
    // taskbar progress should remain unchanged from previous value
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(2));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(80));

    stateMachine.ProcessString(L"\x1b]9;4;3;75\x9c");
    // Given progress value should be ignored because this is the indeterminate state,
    // so the progress value should remain unchanged
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(3));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(80));

    stateMachine.ProcessString(L"\x1b]9;4;0;50\x9c");
    // Taskbar progress should be 0 (the given value should be ignored)
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(0));
    VERIFY_ARE_EQUAL(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));

    stateMachine.ProcessString(L"\x1b]9;4;2;\x9c");
    // String should be processed correctly despite the trailing semicolon,
    // taskbar progress should be set to a 'minimum', non-zero value
    VERIFY_ARE_EQUAL(term.GetTaskbarState(), gsl::narrow<size_t>(2));
    VERIFY_IS_GREATER_THAN(term.GetTaskbarProgress(), gsl::narrow<size_t>(0));
}

void TerminalCoreUnitTests::TerminalApiTest::SetWorkingDirectory()
{
    Terminal term;
    DummyRenderTarget emptyRT;
    term.Create({ 100, 100 }, 0, emptyRT);

    auto& stateMachine = *(term._stateMachine);

    // Test setting working directory using OSC 9;9
    // Initial CWD should be empty
    VERIFY_IS_TRUE(term.GetWorkingDirectory().empty());

    // Invalid sequences should not change CWD
    stateMachine.ProcessString(L"\x1b]9;9\x9c");
    VERIFY_IS_TRUE(term.GetWorkingDirectory().empty());

    stateMachine.ProcessString(L"\x1b]9;9\"\x9c");
    VERIFY_IS_TRUE(term.GetWorkingDirectory().empty());

    stateMachine.ProcessString(L"\x1b]9;9\"C:\\\"\x9c");
    VERIFY_IS_TRUE(term.GetWorkingDirectory().empty());

    // Valid sequences should change CWD
    stateMachine.ProcessString(L"\x1b]9;9;\"C:\\\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"C:\\");

    stateMachine.ProcessString(L"\x1b]9;9;\"C:\\Program Files\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"C:\\Program Files");

    stateMachine.ProcessString(L"\x1b]9;9;\"D:\\中文\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"D:\\中文");

    // Test OSC 9;9 sequences without quotation marks
    stateMachine.ProcessString(L"\x1b]9;9;C:\\\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"C:\\");

    stateMachine.ProcessString(L"\x1b]9;9;C:\\Program Files\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"C:\\Program Files");

    stateMachine.ProcessString(L"\x1b]9;9;D:\\中文\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"D:\\中文");

    // These OSC 9;9 sequences will result in invalid CWD. We shouldn't crash on these.
    stateMachine.ProcessString(L"\x1b]9;9;\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"\"");

    stateMachine.ProcessString(L"\x1b]9;9;\"\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"\"\"");

    stateMachine.ProcessString(L"\x1b]9;9;\"\"\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"\"");

    stateMachine.ProcessString(L"\x1b]9;9;\"\"\"\"\x9c");
    VERIFY_ARE_EQUAL(term.GetWorkingDirectory(), L"\"\"");
}
