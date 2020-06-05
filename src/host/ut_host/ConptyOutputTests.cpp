// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"
#include "../../types/inc/Viewport.hpp"

#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"
#include "../../renderer/vt/XtermEngine.hpp"
#include "../../renderer/vt/WinTelnetEngine.hpp"
#include "../Settings.hpp"

#include "CommonState.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

class ConptyOutputTests
{
    // !!! DANGER: Many tests in this class expect the Terminal and Host buffers
    // to be 80x32. If you change these, you'll probably inadvertently break a
    // bunch of tests !!!
    static const SHORT TerminalViewWidth = 80;
    static const SHORT TerminalViewHeight = 32;

    // This test class is to write some things into the PTY and then check that
    // the rendering that is coming out of the VT-sequence generator is exactly
    // as we expect it to be.
    BEGIN_TEST_CLASS(ConptyOutputTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Class")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = std::make_unique<CommonState>();

        m_state->InitEvents();
        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer(TerminalViewWidth, TerminalViewHeight, TerminalViewWidth, TerminalViewHeight);
        m_state->PrepareGlobalInputBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        m_state.release();

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // Set up some sane defaults
        auto& g = ServiceLocator::LocateGlobals();
        auto& gci = g.getConsoleInformation();
        gci.SetDefaultForegroundColor(INVALID_COLOR);
        gci.SetDefaultBackgroundColor(INVALID_COLOR);
        gci.SetFillAttribute(0x07); // DARK_WHITE on DARK_BLACK

        m_state->PrepareNewTextBufferInfo(true, TerminalViewWidth, TerminalViewHeight);
        auto& currentBuffer = gci.GetActiveOutputBuffer();
        // Make sure a test hasn't left us in the alt buffer on accident
        VERIFY_IS_FALSE(currentBuffer._IsAltBuffer());
        VERIFY_SUCCEEDED(currentBuffer.SetViewportOrigin(true, { 0, 0 }, true));
        VERIFY_ARE_EQUAL(COORD({ 0, 0 }), currentBuffer.GetTextBuffer().GetCursor().GetPosition());

        g.pRender = new Renderer(&gci.renderData, nullptr, 0, nullptr);

        // Set up an xterm-256 renderer for conpty
        wil::unique_hfile hFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
        Viewport initialViewport = currentBuffer.GetViewport();

        auto vtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hFile),
                                                               gci,
                                                               initialViewport,
                                                               gci.Get16ColorTable());
        auto pfn = std::bind(&ConptyOutputTests::_writeCallback, this, std::placeholders::_1, std::placeholders::_2);
        vtRenderEngine->SetTestCallback(pfn);

        g.pRender->AddRenderEngine(vtRenderEngine.get());
        gci.GetActiveOutputBuffer().SetTerminalConnection(vtRenderEngine.get());

        expectedOutput.clear();

        // Manually set the console into conpty mode. We're not actually going
        // to set up the pipes for conpty, but we want the console to behave
        // like it would in conpty mode.
        g.EnableConptyModeForTests(std::move(vtRenderEngine));

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();

        auto& g = ServiceLocator::LocateGlobals();
        delete g.pRender;

        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Tests should drain all the output they push into the expected output buffer.");

        return true;
    }

    TEST_METHOD(ConptyOutputTestCanary);
    TEST_METHOD(SimpleWriteOutputTest);
    TEST_METHOD(WriteTwoLinesUsesNewline);
    TEST_METHOD(WriteAFewSimpleLines);
    TEST_METHOD(InvalidateUntilOneBeforeEnd);

private:
    bool _writeCallback(const char* const pch, size_t const cch);
    void _flushFirstFrame();
    std::deque<std::string> expectedOutput;
    std::unique_ptr<CommonState> m_state;
};

bool ConptyOutputTests::_writeCallback(const char* const pch, size_t const cch)
{
    // Since rendering happens on a background thread that doesn't have the exception handler on it
    // we need to rely on VERIFY's return codes instead of exceptions.
    const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

    std::string actualString = std::string(pch, cch);
    RETURN_BOOL_IF_FALSE(VERIFY_IS_GREATER_THAN(expectedOutput.size(),
                                                static_cast<size_t>(0),
                                                NoThrowString().Format(L"writing=\"%hs\", expecting %u strings", actualString.c_str(), expectedOutput.size())));

    std::string first = expectedOutput.front();
    expectedOutput.pop_front();

    Log::Comment(NoThrowString().Format(L"Expected =\t\"%hs\"", first.c_str()));
    Log::Comment(NoThrowString().Format(L"Actual =\t\"%hs\"", actualString.c_str()));

    RETURN_BOOL_IF_FALSE(VERIFY_ARE_EQUAL(first.length(), cch));
    RETURN_BOOL_IF_FALSE(VERIFY_ARE_EQUAL(first, actualString));

    return true;
}

void ConptyOutputTests::_flushFirstFrame()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;

    expectedOutput.push_back("\x1b[2J");
    expectedOutput.push_back("\x1b[m");
    expectedOutput.push_back("\x1b[H"); // Go Home
    expectedOutput.push_back("\x1b[?25h");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}

// Function Description:
// - Helper function to validate that a number of characters in a row are all
//   the same. Validates that the next end-start characters are all equal to the
//   provided string. Will move the provided iterator as it validates. The
//   caller should ensure that `iter` starts where they would like to validate.
// Arguments:
// - expectedChar: The character (or characters) we're expecting
// - iter: a iterator pointing to the cell we'd like to start validating at.
// - start: the first index in the range we'd like to validate
// - end: the last index in the range we'd like to validate
// Return Value:
// - <none>
void _verifySpanOfText(const wchar_t* const expectedChar,
                       TextBufferCellIterator& iter,
                       const int start,
                       const int end)
{
    for (int x = start; x < end; x++)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
        if (iter->Chars() != expectedChar)
        {
            Log::Comment(NoThrowString().Format(L"character [%d] was mismatched", x));
        }
        VERIFY_ARE_EQUAL(expectedChar, (iter++)->Chars());
    }
    Log::Comment(NoThrowString().Format(
        L"Successfully validated %d characters were '%s'", end - start, expectedChar));
}

void ConptyOutputTests::ConptyOutputTestCanary()
{
    Log::Comment(NoThrowString().Format(
        L"This is a simple test to make sure that everything is working as expected."));

    _flushFirstFrame();
}

void ConptyOutputTests::SimpleWriteOutputTest()
{
    Log::Comment(NoThrowString().Format(
        L"Write some simple output, and make sure it gets rendered largely "
        L"unmodified to the terminal"));

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();

    _flushFirstFrame();

    expectedOutput.push_back("Hello World");
    sm.ProcessString(L"Hello World");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}

void ConptyOutputTests::WriteTwoLinesUsesNewline()
{
    Log::Comment(NoThrowString().Format(
        L"Write two lines of output. We should use \r\n to move the cursor"));

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto& tb = si.GetTextBuffer();

    _flushFirstFrame();

    sm.ProcessString(L"AAA");
    sm.ProcessString(L"\x1b[2;1H");
    sm.ProcessString(L"BBB");

    {
        auto iter = tb.GetCellDataAt({ 0, 0 });
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
    }
    {
        auto iter = tb.GetCellDataAt({ 0, 1 });
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
    }

    expectedOutput.push_back("AAA");
    expectedOutput.push_back("\r\n");
    expectedOutput.push_back("BBB");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}

void ConptyOutputTests::WriteAFewSimpleLines()
{
    Log::Comment(NoThrowString().Format(
        L"Write more lines of output. We should use \r\n to move the cursor"));

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto& tb = si.GetTextBuffer();

    _flushFirstFrame();

    sm.ProcessString(L"AAA\n");
    sm.ProcessString(L"BBB\n");
    sm.ProcessString(L"\n");
    sm.ProcessString(L"CCC");

    {
        auto iter = tb.GetCellDataAt({ 0, 0 });
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"A", (iter++)->Chars());
    }
    {
        auto iter = tb.GetCellDataAt({ 0, 1 });
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
    }
    {
        auto iter = tb.GetCellDataAt({ 0, 2 });
        VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
    }
    {
        auto iter = tb.GetCellDataAt({ 0, 3 });
        VERIFY_ARE_EQUAL(L"C", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"C", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"C", (iter++)->Chars());
    }

    expectedOutput.push_back("AAA");
    expectedOutput.push_back("\r\n");
    expectedOutput.push_back("BBB");
    // Jump down to the fourth line because emitting spaces didn't do anything
    // and we will skip to emitting the CCC segment.
    expectedOutput.push_back("\x1b[4;1H");
    expectedOutput.push_back("CCC");

    // Cursor goes back on.
    expectedOutput.push_back("\x1b[?25h");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}

void ConptyOutputTests::InvalidateUntilOneBeforeEnd()
{
    Log::Comment(NoThrowString().Format(
        L"Make sure we don't use EL and wipe out the last column of text"));

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto& tb = si.GetTextBuffer();

    _flushFirstFrame();

    // Move the cursor to width-15, draw 15 characters
    sm.ProcessString(L"\x1b[1;66H");
    sm.ProcessString(L"ABCDEFGHIJKLMNO");

    {
        auto iter = tb.GetCellDataAt({ 78, 0 });
        VERIFY_ARE_EQUAL(L"N", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"O", (iter++)->Chars());
    }

    expectedOutput.push_back("\x1b[65C");
    expectedOutput.push_back("ABCDEFGHIJKLMNO");

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // overstrike the first with X and the middle 8 with spaces
    sm.ProcessString(L"\x1b[1;66H");
    //                 ABCDEFGHIJKLMNO
    sm.ProcessString(L"X             ");

    {
        auto iter = tb.GetCellDataAt({ 78, 0 });
        VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
        VERIFY_ARE_EQUAL(L"O", (iter++)->Chars());
    }

    expectedOutput.push_back("\x1b[1;66H");
    expectedOutput.push_back("X"); // sequence optimizer should choose ECH here
    expectedOutput.push_back("\x1b[13X");
    expectedOutput.push_back("\x1b[13C");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}
