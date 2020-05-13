// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace winrt::Microsoft::Terminal::Settings;
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
