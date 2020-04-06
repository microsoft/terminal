// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This test class creates an in-proc conpty host as well as a Terminal, to
// validate that strings written to the conpty create the same response on the
// terminal end. Tests can be written that validate both the contents of the
// host buffer as well as the terminal buffer. Everytime that
// `renderer.PaintFrame()` is called, the tests will validate the expected
// output, and then flush the output of the VtEngine straight to the Terminal.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/convert.hpp"

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"
#include "../../renderer/vt/XtermEngine.hpp"
#include "../../renderer/vt/WinTelnetEngine.hpp"

class InputBuffer; // This for some reason needs to be fwd-decl'd
#include "../host/inputBuffer.hpp"
#include "../host/readDataCooked.hpp"
#include "../host/output.h"
#include "test/CommonState.hpp"

#include "../cascadia/TerminalCore/Terminal.hpp"

#include "TestUtils.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

using namespace Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
    class ConptyRoundtripTests;
};
using namespace TerminalCoreUnitTests;

// Helper for declaring a variable to store a TEST_METHOD_PROPERTY and get it's value from the test metadata
#define INIT_TEST_PROPERTY(type, identifer, description) \
    type identifer;                                      \
    VERIFY_SUCCEEDED(TestData::TryGetValue(L#identifer, identifer), description);

class TerminalCoreUnitTests::ConptyRoundtripTests final
{
    static const SHORT TerminalViewWidth = 80;
    static const SHORT TerminalViewHeight = 32;

    // This test class is for tests that are supposed to emit something in the PTY layer
    // and then check that they've been staged for presentation correctly inside
    // the Terminal application. Which sequences were used to get here don't matter.
    TEST_CLASS(ConptyRoundtripTests);

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
        // STEP 1: Set up the Terminal
        term = std::make_unique<Terminal>();
        term->Create({ TerminalViewWidth, TerminalViewHeight }, 100, emptyRT);

        // STEP 2: Set up the Conpty

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

        _pVtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hFile),
                                                            gci,
                                                            initialViewport,
                                                            gci.GetColorTable(),
                                                            static_cast<WORD>(gci.GetColorTableSize()));
        auto pfn = std::bind(&ConptyRoundtripTests::_writeCallback, this, std::placeholders::_1, std::placeholders::_2);
        _pVtRenderEngine->SetTestCallback(pfn);

        // Enable the resize quirk, as the Terminal is going to be reacting as if it's enabled.
        _pVtRenderEngine->SetResizeQuirk(true);

        // Configure the OutputStateMachine's _pfnFlushToTerminal
        // Use OutputStateMachineEngine::SetTerminalConnection
        g.pRender->AddRenderEngine(_pVtRenderEngine.get());
        gci.GetActiveOutputBuffer().SetTerminalConnection(_pVtRenderEngine.get());

        _pConApi = std::make_unique<ConhostInternalGetSet>(gci);

        // Manually set the console into conpty mode. We're not actually going
        // to set up the pipes for conpty, but we want the console to behave
        // like it would in conpty mode.
        g.EnableConptyModeForTests();

        expectedOutput.clear();
        _checkConptyOutput = true;
        _logConpty = false;

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();

        auto& g = ServiceLocator::LocateGlobals();
        delete g.pRender;

        VERIFY_ARE_EQUAL(0u, expectedOutput.size(), L"Tests should drain all the output they push into the expected output buffer.");

        term = nullptr;

        return true;
    }

    TEST_METHOD(ConptyOutputTestCanary);
    TEST_METHOD(SimpleWriteOutputTest);
    TEST_METHOD(WriteTwoLinesUsesNewline);
    TEST_METHOD(WriteAFewSimpleLines);

    TEST_METHOD(PassthroughClearScrollback);

    TEST_METHOD(PassthroughHardReset);

    TEST_METHOD(PassthroughCursorShapeImmediately);

    TEST_METHOD(TestWrappingALongString);
    TEST_METHOD(TestAdvancedWrapping);
    TEST_METHOD(TestExactWrappingWithoutSpaces);
    TEST_METHOD(TestExactWrappingWithSpaces);

    TEST_METHOD(MoveCursorAtEOL);

    TEST_METHOD(TestResizeHeight);

    TEST_METHOD(OutputWrappedLinesAtTopOfBuffer);
    TEST_METHOD(OutputWrappedLinesAtBottomOfBuffer);
    TEST_METHOD(ScrollWithChangesInMiddle);
    TEST_METHOD(DontWrapMoveCursorInSingleFrame);
    TEST_METHOD(ClearHostTrickeryTest);
    TEST_METHOD(OverstrikeAtBottomOfBuffer);
    TEST_METHOD(MarginsWithStatusLine);

    TEST_METHOD(ScrollWithMargins);

private:
    bool _writeCallback(const char* const pch, size_t const cch);
    void _flushFirstFrame();
    void _resizeConpty(const unsigned short sx, const unsigned short sy);
    std::deque<std::string> expectedOutput;
    std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
    std::unique_ptr<CommonState> m_state;
    std::unique_ptr<Microsoft::Console::VirtualTerminal::ConGetSet> _pConApi;

    // Tests can set these variables how they link to configure the behavior of the test harness.
    bool _checkConptyOutput{ true }; // If true, the test class will check that the output from conpty was expected
    bool _logConpty{ false }; // If true, the test class will log all the output from conpty. Helpful for debugging.

    DummyRenderTarget emptyRT;
    std::unique_ptr<Terminal> term;
};

bool ConptyRoundtripTests::_writeCallback(const char* const pch, size_t const cch)
{
    std::string actualString = std::string(pch, cch);

    if (_checkConptyOutput)
    {
        VERIFY_IS_GREATER_THAN(expectedOutput.size(),
                               static_cast<size_t>(0),
                               NoThrowString().Format(L"writing=\"%hs\", expecting %u strings", TestUtils::ReplaceEscapes(actualString).c_str(), expectedOutput.size()));

        std::string first = expectedOutput.front();
        expectedOutput.pop_front();

        Log::Comment(NoThrowString().Format(L"Expected =\t\"%hs\"", TestUtils::ReplaceEscapes(first).c_str()));
        Log::Comment(NoThrowString().Format(L"Actual =\t\"%hs\"", TestUtils::ReplaceEscapes(actualString).c_str()));

        VERIFY_ARE_EQUAL(first.length(), cch);
        VERIFY_ARE_EQUAL(first, actualString);
    }
    else if (_logConpty)
    {
        Log::Comment(NoThrowString().Format(
            L"Writing \"%hs\" to Terminal", TestUtils::ReplaceEscapes(actualString).c_str()));
    }

    // Write the string back to our Terminal
    const auto converted = ConvertToW(CP_UTF8, actualString);
    term->Write(converted);

    return true;
}

void ConptyRoundtripTests::_flushFirstFrame()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;

    expectedOutput.push_back("\x1b[2J");
    expectedOutput.push_back("\x1b[m");
    expectedOutput.push_back("\x1b[H"); // Go Home
    expectedOutput.push_back("\x1b[?25h");

    VERIFY_SUCCEEDED(renderer.PaintFrame());
}

void ConptyRoundtripTests::_resizeConpty(const unsigned short sx,
                                         const unsigned short sy)
{
    // Largely taken from implementation in PtySignalInputThread::_InputThread
    if (DispatchCommon::s_ResizeWindow(*_pConApi, sx, sy))
    {
        // Instead of going through the VtIo to suppress the resize repaint,
        // just call the method directly on the renderer. This is implemented in
        // VtIo::SuppressResizeRepaint
        VERIFY_SUCCEEDED(_pVtRenderEngine->SuppressResizeRepaint());
    }
}

void ConptyRoundtripTests::ConptyOutputTestCanary()
{
    Log::Comment(NoThrowString().Format(
        L"This is a simple test to make sure that everything is working as expected."));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    _flushFirstFrame();
}

void ConptyRoundtripTests::SimpleWriteOutputTest()
{
    Log::Comment(NoThrowString().Format(
        L"Write some simple output, and make sure it gets rendered largely "
        L"unmodified to the terminal"));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    expectedOutput.push_back("Hello World");
    hostSm.ProcessString(L"Hello World");

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    TestUtils::VerifyExpectedString(termTb, L"Hello World ", { 0, 0 });
}

void ConptyRoundtripTests::WriteTwoLinesUsesNewline()
{
    Log::Comment(NoThrowString().Format(
        L"Write two lines of output. We should use \r\n to move the cursor"));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    hostSm.ProcessString(L"AAA");
    hostSm.ProcessString(L"\x1b[2;1H");
    hostSm.ProcessString(L"BBB");

    auto verifyData = [](TextBuffer& tb) {
        TestUtils::VerifyExpectedString(tb, L"AAA", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"BBB", { 0, 1 });
    };

    verifyData(hostTb);

    expectedOutput.push_back("AAA");
    expectedOutput.push_back("\r\n");
    expectedOutput.push_back("BBB");

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyData(termTb);
}

void ConptyRoundtripTests::WriteAFewSimpleLines()
{
    Log::Comment(NoThrowString().Format(
        L"Write more lines of outout. We should use \r\n to move the cursor"));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    hostSm.ProcessString(L"AAA\n");
    hostSm.ProcessString(L"BBB\n");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"CCC");
    auto verifyData = [](TextBuffer& tb) {
        TestUtils::VerifyExpectedString(tb, L"AAA", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"BBB", { 0, 1 });
        TestUtils::VerifyExpectedString(tb, L"   ", { 0, 2 });
        TestUtils::VerifyExpectedString(tb, L"CCC", { 0, 3 });
    };

    verifyData(hostTb);

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

    verifyData(termTb);
}

void ConptyRoundtripTests::TestWrappingALongString()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();
    _checkConptyOutput = false;

    const auto initialTermView = term->GetViewport();

    const auto charsToWrite = gsl::narrow_cast<short>(TestUtils::Test100CharsString.size());
    VERIFY_ARE_EQUAL(100, charsToWrite);

    VERIFY_ARE_EQUAL(0, initialTermView.Top());
    VERIFY_ARE_EQUAL(32, initialTermView.BottomExclusive());

    hostSm.ProcessString(TestUtils::Test100CharsString);

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, secondView.Top());
    VERIFY_ARE_EQUAL(32, secondView.BottomExclusive());

    auto verifyBuffer = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(charsToWrite % initialTermView.Width(), cursor.GetPosition().X);
        VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);

        // Verify that we marked the 0th row as _wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_IS_TRUE(row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, TestUtils::Test100CharsString, { 0, 0 });
    };

    verifyBuffer(hostTb);

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb);
}

void ConptyRoundtripTests::TestAdvancedWrapping()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    const auto initialTermView = term->GetViewport();

    _flushFirstFrame();

    const auto charsToWrite = gsl::narrow_cast<short>(TestUtils::Test100CharsString.size());
    VERIFY_ARE_EQUAL(100, charsToWrite);

    hostSm.ProcessString(TestUtils::Test100CharsString);
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"          ");
    hostSm.ProcessString(L"1234567890");

    auto verifyBuffer = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(2, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(20, cursor.GetPosition().X);

        // Verify that we marked the 0th row as _wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_IS_TRUE(row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, TestUtils::Test100CharsString, { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"          1234567890", { 0, 2 });
    };

    verifyBuffer(hostTb);

    // First write the first 80 characters from the string
    expectedOutput.push_back(R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)");
    // Without line breaking, write the remaining 20 chars
    expectedOutput.push_back(R"(qrstuvwxyz{|}~!"#$%&)");
    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("          1234567890");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb);
}

void ConptyRoundtripTests::TestExactWrappingWithoutSpaces()
{
    // This test (and TestExactWrappingWitSpaces) reveals a bug in the old
    // implementation.
    //
    // If a line _exactly_ wraps to the next line, we can't tell if the line
    // should really wrap, or manually break. The client app is writing a line
    // that's exactly the width of the buffer that manually breaks at the
    // end of the line, followed by another line.
    //
    // With the old PaintBufferLine interface, there's no way to know if this
    // case is because the line wrapped or not. Hence, the addition of the
    // `lineWrapped` parameter

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    const auto initialTermView = term->GetViewport();

    _flushFirstFrame();

    const auto charsToWrite = initialTermView.Width();
    VERIFY_ARE_EQUAL(80, charsToWrite);

    for (auto i = 0; i < charsToWrite; i++)
    {
        // This is a handy way of just printing the printable characters that
        // _aren't_ the space character.
        const wchar_t wch = static_cast<wchar_t>(33 + (i % 94));
        hostSm.ProcessCharacter(wch);
    }

    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"1234567890");

    auto verifyBuffer = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(10, cursor.GetPosition().X);

        // Verify that we marked the 0th row as _not wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_IS_FALSE(row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, LR"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"1234567890", { 0, 1 });
    };

    verifyBuffer(hostTb);

    // First write the first 80 characters from the string
    expectedOutput.push_back(R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)");

    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("1234567890");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb);
}

void ConptyRoundtripTests::TestExactWrappingWithSpaces()
{
    // This test is also explained by the comment at the top of TestExactWrappingWithoutSpaces

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    const auto initialTermView = term->GetViewport();

    _flushFirstFrame();

    const auto charsToWrite = initialTermView.Width();
    VERIFY_ARE_EQUAL(80, charsToWrite);

    for (auto i = 0; i < charsToWrite; i++)
    {
        // This is a handy way of just printing the printable characters that
        // _aren't_ the space character.
        const wchar_t wch = static_cast<wchar_t>(33 + (i % 94));
        hostSm.ProcessCharacter(wch);
    }

    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"          ");
    hostSm.ProcessString(L"1234567890");

    auto verifyBuffer = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(20, cursor.GetPosition().X);

        // Verify that we marked the 0th row as _not wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_IS_FALSE(row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, LR"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"          1234567890", { 0, 1 });
    };

    verifyBuffer(hostTb);

    // First write the first 80 characters from the string
    expectedOutput.push_back(R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)");

    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("          1234567890");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb);
}

void ConptyRoundtripTests::MoveCursorAtEOL()
{
    // This is a test for GH#1245
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    _flushFirstFrame();

    Log::Comment(NoThrowString().Format(
        L"Write exactly a full line of text"));
    hostSm.ProcessString(std::wstring(TerminalViewWidth, L'A'));

    auto verifyData0 = [](TextBuffer& tb) {
        auto iter = tb.GetCellDataAt({ 0, 0 });
        TestUtils::VerifySpanOfText(L"A", iter, 0, TerminalViewWidth);
        TestUtils::VerifySpanOfText(L" ", iter, 0, TerminalViewWidth);
    };

    verifyData0(hostTb);

    // TODO: GH#405/#4415 - Before #405 merges, the VT sequences conpty emits
    // might change, but the buffer contents shouldn't.
    // If they do change and these tests break, that's to be expected.
    expectedOutput.push_back(std::string(TerminalViewWidth, 'A'));
    expectedOutput.push_back("\x1b[1;80H");

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyData0(termTb);

    Log::Comment(NoThrowString().Format(
        L"Emulate backspacing at a bash prompt when the previous line wrapped.\n"
        L"We'll move the cursor up to the last char of the prev line, and erase it."));
    hostSm.ProcessString(L"\x1b[1;80H");
    hostSm.ProcessString(L"\x1b[K");

    auto verifyData1 = [](TextBuffer& tb) {
        auto iter = tb.GetCellDataAt({ 0, 0 });
        // There should be 79 'A's, followed by a space, and the following line should be blank.
        TestUtils::VerifySpanOfText(L"A", iter, 0, TerminalViewWidth - 1);
        TestUtils::VerifySpanOfText(L" ", iter, 0, 1);
        TestUtils::VerifySpanOfText(L" ", iter, 0, TerminalViewWidth);

        auto& cursor = tb.GetCursor();
        VERIFY_ARE_EQUAL(TerminalViewWidth - 1, cursor.GetPosition().X);
        VERIFY_ARE_EQUAL(0, cursor.GetPosition().Y);
    };

    verifyData1(hostTb);

    expectedOutput.push_back(" ");
    expectedOutput.push_back("\x1b[1;80H");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyData1(termTb);
}

void ConptyRoundtripTests::TestResizeHeight()
{
    // This test class is _60_ tests to ensure that resizing the terminal works
    // with conpty correctly. There's a lot of min/maxing in expressions here,
    // to account for the sheer number of cases here, and that we have to handle
    // both resizing larger and smaller all in one test.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-1, 0, 1}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
        TEST_METHOD_PROPERTY(L"Data:printedRows", L"{1, 10, 50, 200}")
    END_TEST_METHOD_PROPERTIES()
    int dx, dy;
    int printedRows;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dx", dx), L"change in width of buffer");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"printedRows", printedRows), L"Number of rows of text to print");

    _checkConptyOutput = false;

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();
    const auto initialHostView = si.GetViewport();
    const auto initialTermView = term->GetViewport();
    const auto initialTerminalBufferHeight = term->GetTextBuffer().GetSize().Height();

    VERIFY_ARE_EQUAL(0, initialHostView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, initialHostView.BottomExclusive());
    VERIFY_ARE_EQUAL(0, initialTermView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, initialTermView.BottomExclusive());

    Log::Comment(NoThrowString().Format(
        L"Print %d lines of output, which will scroll the viewport", printedRows));

    for (auto i = 0; i < printedRows; i++)
    {
        // This looks insane, but this expression is carefully crafted to give
        // us only printable characters, starting with `!` (0n33).
        // Similar statements are used elsewhere throughout this test.
        auto wstr = std::wstring(1, static_cast<wchar_t>((i) % 93) + 33);
        hostSm.ProcessString(wstr);
        hostSm.ProcessString(L"\r\n");
    }

    // Conpty doesn't have a scrollback, it's view's origin is always 0,0
    const auto secondHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, secondHostView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, secondHostView.BottomExclusive());

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    const auto secondTermView = term->GetViewport();
    // If we've printed more lines than the height of the buffer, then we're
    // expecting the viewport to have moved down. Otherwise, the terminal's
    // viewport will stay at 0,0.
    const auto expectedTerminalViewBottom = std::max(std::min(::base::saturated_cast<short>(printedRows + 1),
                                                              term->GetBufferHeight()),
                                                     term->GetViewport().Height());

    VERIFY_ARE_EQUAL(expectedTerminalViewBottom, secondTermView.BottomExclusive());
    VERIFY_ARE_EQUAL(expectedTerminalViewBottom - initialTermView.Height(), secondTermView.Top());

    auto verifyTermData = [&expectedTerminalViewBottom, &printedRows, this, &initialTerminalBufferHeight](TextBuffer& termTb, const int resizeDy = 0) {
        // Some number of lines of text were lost from the scrollback. The
        // number of lines lost will be determined by whichever of the initial
        // or current buffer is smaller.
        const auto numLostRows = std::max(0,
                                          printedRows - std::min(term->GetTextBuffer().GetSize().Height(), initialTerminalBufferHeight) + 1);

        const auto rowsWithText = std::min(::base::saturated_cast<short>(printedRows),
                                           expectedTerminalViewBottom) -
                                  1 + std::min(resizeDy, 0);

        for (short row = 0; row < rowsWithText; row++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = termTb.GetCellDataAt({ 0, row });
            const wchar_t expectedChar = static_cast<wchar_t>((row + numLostRows) % 93) + 33;

            auto expectedString = std::wstring(1, expectedChar);

            if (iter->Chars() != expectedString)
            {
                Log::Comment(NoThrowString().Format(L"row [%d] was mismatched", row));
            }
            VERIFY_ARE_EQUAL(expectedString, (iter++)->Chars());
            VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }
    };
    auto verifyHostData = [&si, &initialHostView, &printedRows](TextBuffer& hostTb, const int resizeDy = 0) {
        const auto hostView = si.GetViewport();

        // In the host, there are two regions we're interested in:

        // 1. the first section of the buffer with the output in it. Before
        //    we're resized, this will be filled with one character on each row.
        // 2. The second area below the first that's empty (filled with spaces).
        //    Initially, this is only one row.
        // After we resize, different things will happen.
        // * If we decrease the height of the buffer, the characters in the
        //   buffer will all move _up_ the same number of rows. We'll want to
        //   only check the first initialView+dy rows for characters.
        // * If we increase the height, rows will be added at the bottom. We'll
        //   want to check the initial viewport height for the original
        //   characters, but then we'll want to look for more blank rows at the
        //   bottom. The characters in the initial viewport won't have moved.

        const short originalViewHeight = ::base::saturated_cast<short>(resizeDy < 0 ?
                                                                           initialHostView.Height() + resizeDy :
                                                                           initialHostView.Height());
        const auto rowsWithText = std::min(originalViewHeight - 1, printedRows);
        const bool scrolled = printedRows > initialHostView.Height();
        // The last row of the viewport should be empty
        // The second last row will have '0'+50
        // The third last row will have '0'+49
        // ...
        // The <height> last row will have '0'+(50-height+1)
        const auto firstChar = static_cast<wchar_t>(scrolled ?
                                                        (printedRows - originalViewHeight + 1) :
                                                        0);

        short row = 0;
        // Don't include the last row of the viewport in this check, since it'll
        // be blank. We'll check it in the below loop.
        for (; row < rowsWithText; row++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = hostTb.GetCellDataAt({ 0, row });

            const auto expectedChar = static_cast<wchar_t>(((firstChar + row) % 93) + 33);
            auto expectedString = std::wstring(1, static_cast<wchar_t>(expectedChar));

            if (iter->Chars() != expectedString)
            {
                Log::Comment(NoThrowString().Format(L"row [%d] was mismatched", row));
            }
            VERIFY_ARE_EQUAL(expectedString, (iter++)->Chars(), NoThrowString().Format(L"%s", expectedString.data()));
            VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }

        // Check that the remaining rows in the viewport are empty.
        for (; row < hostView.Height(); row++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = hostTb.GetCellDataAt({ 0, row });
            VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }
    };

    verifyHostData(*hostTb);
    verifyTermData(*termTb);

    const COORD newViewportSize{
        ::base::saturated_cast<short>(TerminalViewWidth + dx),
        ::base::saturated_cast<short>(TerminalViewHeight + dy)
    };

    Log::Comment(NoThrowString().Format(L"Resize the Terminal and conpty here"));
    auto resizeResult = term->UserResize(newViewportSize);
    VERIFY_SUCCEEDED(resizeResult);
    _resizeConpty(newViewportSize.X, newViewportSize.Y);

    // After we resize, make sure to get the new textBuffers
    hostTb = &si.GetTextBuffer();
    termTb = term->_buffer.get();

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto thirdHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, thirdHostView.Top());
    VERIFY_ARE_EQUAL(newViewportSize.Y, thirdHostView.BottomExclusive());

    // The Terminal should be stuck to the top of the viewport, unless dy<0,
    // rows=50. In that set of cases, we _didn't_ pin the top of the Terminal to
    // the old top, we actually shifted it down (because the output was at the
    // bottom of the window, not empty lines).
    const auto thirdTermView = term->GetViewport();
    if (dy < 0 && (printedRows > initialTermView.Height() && printedRows < initialTerminalBufferHeight))
    {
        VERIFY_ARE_EQUAL(secondTermView.Top() - dy, thirdTermView.Top());
        VERIFY_ARE_EQUAL(expectedTerminalViewBottom, thirdTermView.BottomExclusive());
    }
    else
    {
        VERIFY_ARE_EQUAL(secondTermView.Top(), thirdTermView.Top());
        VERIFY_ARE_EQUAL(expectedTerminalViewBottom + dy, thirdTermView.BottomExclusive());
    }

    verifyHostData(*hostTb, dy);
    // Note that at this point, nothing should have changed with the Terminal.
    verifyTermData(*termTb, dy);

    Log::Comment(NoThrowString().Format(L"Paint a frame to update the Terminal"));
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto fourthHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, fourthHostView.Top());
    VERIFY_ARE_EQUAL(newViewportSize.Y, fourthHostView.BottomExclusive());

    // The Terminal should be stuck to the top of the viewport, unless dy<0,
    // rows=50. In that set of cases, we _didn't_ pin the top of the Terminal to
    // the old top, we actually shifted it down (because the output was at the
    // bottom of the window, not empty lines).
    const auto fourthTermView = term->GetViewport();
    if (dy < 0 && (printedRows > initialTermView.Height() && printedRows < initialTerminalBufferHeight))
    {
        VERIFY_ARE_EQUAL(secondTermView.Top() - dy, thirdTermView.Top());
        VERIFY_ARE_EQUAL(expectedTerminalViewBottom, thirdTermView.BottomExclusive());
    }
    else
    {
        VERIFY_ARE_EQUAL(secondTermView.Top(), thirdTermView.Top());
        VERIFY_ARE_EQUAL(expectedTerminalViewBottom + dy, thirdTermView.BottomExclusive());
    }
    verifyHostData(*hostTb, dy);
    verifyTermData(*termTb, dy);
}

void ConptyRoundtripTests::PassthroughCursorShapeImmediately()
{
    // This is a test for GH#4106, and more indirectly, GH #2011.

    Log::Comment(NoThrowString().Format(
        L"Change the cursor shape with VT. This should immediately be flushed to the Terminal."));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    _logConpty = true;

    VERIFY_ARE_NOT_EQUAL(CursorType::VerticalBar, hostTb.GetCursor().GetType());
    VERIFY_ARE_NOT_EQUAL(CursorType::VerticalBar, termTb.GetCursor().GetType());

    expectedOutput.push_back("\x1b[5 q");
    hostSm.ProcessString(L"\x1b[5 q");

    VERIFY_ARE_EQUAL(CursorType::VerticalBar, hostTb.GetCursor().GetType());
    VERIFY_ARE_EQUAL(CursorType::VerticalBar, termTb.GetCursor().GetType());
}

void ConptyRoundtripTests::PassthroughClearScrollback()
{
    Log::Comment(NoThrowString().Format(
        L"Write more lines of output than there are lines in the viewport. Clear the scrollback with ^[[3J"));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    _logConpty = true;

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        Log::Comment(NoThrowString().Format(L"Writing line %d/%d", i, end));
        expectedOutput.push_back("X");
        if (i < hostView.BottomInclusive())
        {
            expectedOutput.push_back("\r\n");
        }
        else
        {
            // After we hit the bottom of the viewport, the newlines come in
            // separated by empty writes for whatever reason.
            expectedOutput.push_back("\r");
            expectedOutput.push_back("\n");
            expectedOutput.push_back("");
        }

        hostSm.ProcessString(L"X\n");

        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Verify that we've printed height*2 lines of X's to the Terminal
    const auto termFirstView = term->GetViewport();
    for (short y = 0; y < 2 * termFirstView.Height(); y++)
    {
        TestUtils::VerifyExpectedString(termTb, L"X  ", { 0, y });
    }

    // Write a Erase Scrollback VT sequence to the host, it should come through to the Terminal
    expectedOutput.push_back("\x1b[3J");
    hostSm.ProcessString(L"\x1b[3J");

    _checkConptyOutput = false;

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    const auto termSecondView = term->GetViewport();
    VERIFY_ARE_EQUAL(0, termSecondView.Top());

    // Verify the top of the Terminal viewport contains the contents of the old viewport
    for (short y = 0; y < termSecondView.BottomInclusive(); y++)
    {
        TestUtils::VerifyExpectedString(termTb, L"X  ", { 0, y });
    }

    // Verify below the new viewport (the old viewport) has been cleared out
    for (short y = termSecondView.BottomInclusive(); y < termFirstView.BottomInclusive(); y++)
    {
        TestUtils::VerifyExpectedString(termTb, std::wstring(TerminalViewWidth, L' '), { 0, y });
    }
}

void ConptyRoundtripTests::PassthroughHardReset()
{
    // This test is highly similar to PassthroughClearScrollback.
    Log::Comment(NoThrowString().Format(
        L"Write more lines of output than there are lines in the viewport. Clear everything with ^[c"));
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    _logConpty = true;

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        Log::Comment(NoThrowString().Format(L"Writing line %d/%d", i, end));
        expectedOutput.push_back("X");
        if (i < hostView.BottomInclusive())
        {
            expectedOutput.push_back("\r\n");
        }
        else
        {
            // After we hit the bottom of the viewport, the newlines come in
            // separated by empty writes for whatever reason.
            expectedOutput.push_back("\r");
            expectedOutput.push_back("\n");
            expectedOutput.push_back("");
        }

        hostSm.ProcessString(L"X\n");

        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Verify that we've printed height*2 lines of X's to the Terminal
    const auto termFirstView = term->GetViewport();
    for (short y = 0; y < 2 * termFirstView.Height(); y++)
    {
        TestUtils::VerifyExpectedString(termTb, L"X  ", { 0, y });
    }

    // Write a Hard Reset VT sequence to the host, it should come through to the Terminal
    expectedOutput.push_back("\033c");
    hostSm.ProcessString(L"\033c");

    const auto termSecondView = term->GetViewport();
    VERIFY_ARE_EQUAL(0, termSecondView.Top());

    // Verify everything has been cleared out
    for (short y = 0; y < termFirstView.BottomInclusive(); y++)
    {
        TestUtils::VerifyExpectedString(termTb, std::wstring(TerminalViewWidth, L' '), { 0, y });
    }
}

void ConptyRoundtripTests::OutputWrappedLinesAtTopOfBuffer()
{
    Log::Comment(
        L"Case 1: Write a wrapped line right at the start of the buffer, before any circling");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    const auto wrappedLineLength = TerminalViewWidth + 20;

    sm.ProcessString(std::wstring(wrappedLineLength, L'A'));

    auto verifyBuffer = [](const TextBuffer& tb) {
        VERIFY_IS_TRUE(tb.GetRowByOffset(0).GetCharRow().WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(1).GetCharRow().WasWrapForced());
        auto iter0 = tb.GetCellDataAt({ 0, 0 });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, TerminalViewWidth);
        auto iter1 = tb.GetCellDataAt({ 0, 1 });
        TestUtils::VerifySpanOfText(L"A", iter1, 0, 20);
        auto iter2 = tb.GetCellDataAt({ 20, 1 });
        TestUtils::VerifySpanOfText(L" ", iter2, 0, TerminalViewWidth - 20);
    };

    verifyBuffer(hostTb);

    expectedOutput.push_back(std::string(TerminalViewWidth, 'A'));
    expectedOutput.push_back(std::string(20, 'A'));
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb);
}

void ConptyRoundtripTests::OutputWrappedLinesAtBottomOfBuffer()
{
    Log::Comment(
        L"Case 2: Write a wrapped line at the end of the buffer, once the conpty started circling");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    // First, fill the buffer with contents, so conpty starts circling

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        Log::Comment(NoThrowString().Format(L"Writing line %d/%d", i, end));
        expectedOutput.push_back("X");
        if (i < hostView.BottomInclusive())
        {
            expectedOutput.push_back("\r\n");
        }
        else
        {
            // After we hit the bottom of the viewport, the newlines come in
            // separated by empty writes for whatever reason.
            expectedOutput.push_back("\r");
            expectedOutput.push_back("\n");
            expectedOutput.push_back("");
        }

        hostSm.ProcessString(L"X\n");

        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    const auto wrappedLineLength = TerminalViewWidth + 20;

    expectedOutput.push_back(std::string(TerminalViewWidth, 'A'));
    // TODO GH#5228 might break the "newline & repaint the wrapped char" checks here, that's okay.
    expectedOutput.push_back("\r"); // This \r\n is emitted by ScrollFrame to
    expectedOutput.push_back("\n"); // add a newline to the bottom of the buffer
    expectedOutput.push_back("\x1b[31;80H"); // Move the cursor BACK to the wrapped row
    expectedOutput.push_back(std::string(1, 'A')); // Reprint the last character of the wrapped row
    expectedOutput.push_back(std::string(20, 'A')); // Print the second line.

    hostSm.ProcessString(std::wstring(wrappedLineLength, L'A'));

    auto verifyBuffer = [](const TextBuffer& tb, const short wrappedRow) {
        VERIFY_IS_TRUE(tb.GetRowByOffset(wrappedRow).GetCharRow().WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(wrappedRow + 1).GetCharRow().WasWrapForced());

        auto iter0 = tb.GetCellDataAt({ 0, wrappedRow });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, TerminalViewWidth);
        auto iter1 = tb.GetCellDataAt({ 0, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L"A", iter1, 0, 20);
        auto iter2 = tb.GetCellDataAt({ 20, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L" ", iter2, 0, TerminalViewWidth - 20);
    };

    verifyBuffer(hostTb, hostView.BottomInclusive() - 1);

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb, term->_mutableViewport.BottomInclusive() - 1);
}

void ConptyRoundtripTests::ScrollWithChangesInMiddle()
{
    Log::Comment(L"This test checks emitting a wrapped line at the bottom of the"
                 L" viewport while _also_ emitting other text elsewhere in the same frame. This"
                 L" output will cause us to scroll the viewport in one frame, but we need to"
                 L" make sure the wrapped line _stays_ wrapped, and the scrolled text appears in"
                 L" the right place.");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    // First, fill the buffer with contents, so conpty starts circling

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        Log::Comment(NoThrowString().Format(L"Writing line %d/%d", i, end));
        expectedOutput.push_back("X");
        if (i < hostView.BottomInclusive())
        {
            expectedOutput.push_back("\r\n");
        }
        else
        {
            // After we hit the bottom of the viewport, the newlines come in
            // separated by empty writes for whatever reason.
            expectedOutput.push_back("\r");
            expectedOutput.push_back("\n");
            expectedOutput.push_back("");
        }

        hostSm.ProcessString(L"X\n");

        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    const auto wrappedLineLength = TerminalViewWidth + 20;

    // In the Terminal, we're going to expect:
    expectedOutput.push_back("\x1b[15;1H"); // Move the cursor to row 14, col 0
    expectedOutput.push_back("Y"); // Print a 'Y'
    expectedOutput.push_back("\x1b[32;1H"); // Move the cursor to the last row
    expectedOutput.push_back(std::string(TerminalViewWidth, 'A')); // Print the first 80 'A's
    // This is going to be the end of the first frame - b/c we moved the cursor
    // in the middle of the frame, we're going to hide/show the cursor during
    // this frame
    expectedOutput.push_back("\x1b[?25h"); // hide the cursor
    // On the subsequent frame:
    // TODO GH#5228 might break the "newline & repaint the wrapped char" checks here, that's okay.
    expectedOutput.push_back("\r"); // This \r\n is emitted by ScrollFrame to
    expectedOutput.push_back("\n"); // add a newline to the bottom of the buffer
    expectedOutput.push_back("\x1b[31;80H"); // Move the cursor BACK to the wrapped row
    expectedOutput.push_back(std::string(1, 'A')); // Reprint the last character of the wrapped row
    expectedOutput.push_back(std::string(20, 'A')); // Print the second line.

    _logConpty = true;

    // To the host, we'll do something very similar:
    hostSm.ProcessString(L"\x1b"
                         L"7"); // Save cursor
    hostSm.ProcessString(L"\x1b[15;1H"); // Move the cursor to row 14, col 0
    hostSm.ProcessString(L"Y"); // Print a 'Y'
    hostSm.ProcessString(L"\x1b"
                         L"8"); // Restore
    hostSm.ProcessString(std::wstring(wrappedLineLength, L'A')); // Print 100 'A's

    auto verifyBuffer = [](const TextBuffer& tb, const til::rectangle viewport) {
        const short wrappedRow = viewport.bottom<short>() - 2;
        const short start = viewport.top<short>();
        for (short i = start; i < wrappedRow; i++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", i));
            TestUtils::VerifyExpectedString(tb, i == start + 13 ? L"Y" : L"X", { 0, i });
        }

        VERIFY_IS_TRUE(tb.GetRowByOffset(wrappedRow).GetCharRow().WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(wrappedRow + 1).GetCharRow().WasWrapForced());

        auto iter0 = tb.GetCellDataAt({ 0, wrappedRow });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, TerminalViewWidth);
        auto iter1 = tb.GetCellDataAt({ 0, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L"A", iter1, 0, 20);
        auto iter2 = tb.GetCellDataAt({ 20, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L" ", iter2, 0, TerminalViewWidth - 20);
    };

    Log::Comment(NoThrowString().Format(L"Checking the host buffer..."));
    verifyBuffer(hostTb, hostView.ToInclusive());
    Log::Comment(NoThrowString().Format(L"... Done"));

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(NoThrowString().Format(L"Checking the terminal buffer..."));
    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
    Log::Comment(NoThrowString().Format(L"... Done"));
}

void ConptyRoundtripTests::ScrollWithMargins()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    const auto initialTermView = term->GetViewport();

    Log::Comment(L"Flush first frame.");
    _flushFirstFrame();

    // Fill up the buffer with some text.
    // We're going to write something like this:
    // AAAA
    // BBBB
    // CCCC
    // ........
    // QQQQ
    // ****************
    // The letters represent the data in the TMUX pane.
    // The final *** line represents the mode line which we will
    // attempt to hold in place and not scroll.
    // Note that the last line will contain one '*' less than the width of the window.

    Log::Comment(L"Fill host with text pattern by feeding it into VT parser.");
    const auto rowsToWrite = initialTermView.Height() - 1;

    // For all lines but the last one, write out a few of a letter.
    for (auto i = 0; i < rowsToWrite; ++i)
    {
        const wchar_t wch = static_cast<wchar_t>(L'A' + i);
        hostSm.ProcessCharacter(wch);
        hostSm.ProcessCharacter(wch);
        hostSm.ProcessCharacter(wch);
        hostSm.ProcessCharacter(wch);
        hostSm.ProcessCharacter('\n');
    }

    // For the last one, write out the asterisks for the mode line.
    for (auto i = 0; i < initialTermView.Width() - 1; ++i)
    {
        hostSm.ProcessCharacter('*');
    }

    // no newline in the bottom right corner or it will move unexpectedly.

    // Now set up the verification that the buffers are full of the pattern we expect.
    // This function will verify the text backing buffers.
    auto verifyBuffer = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor is waiting in the bottom right corner
        VERIFY_ARE_EQUAL(initialTermView.Height() - 1, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(initialTermView.Width() - 1, cursor.GetPosition().X);

        // For all rows except the last one, verify that we have a run of four letters.
        for (auto i = 0; i < rowsToWrite; ++i)
        {
            const std::wstring expectedString(4, static_cast<wchar_t>(L'A' + i));
            const COORD expectedPos{ 0, gsl::narrow<SHORT>(i) };
            TestUtils::VerifyExpectedString(tb, expectedString, expectedPos);
        }

        // For the last row, verify we have an entire row of asterisks for the mode line.
        const std::wstring expectedModeLine(initialTermView.Width() - 1, L'*');
        const COORD expectedPos{ 0, gsl::narrow<SHORT>(rowsToWrite) };
        TestUtils::VerifyExpectedString(tb, expectedModeLine, expectedPos);
    };

    // This will verify the text emitted from the PTY.
    for (auto i = 0; i < rowsToWrite; ++i)
    {
        const std::string expectedString(4, static_cast<char>('A' + i));
        expectedOutput.push_back(expectedString);
        expectedOutput.push_back("\r\n");
    }
    {
        const std::string expectedString(initialTermView.Width() - 1, '*');
        expectedOutput.push_back(expectedString);
    }

    Log::Comment(L"Verify host buffer contains pattern.");
    // Verify the host side.
    verifyBuffer(hostTb);

    Log::Comment(L"Emit PTY frame and validate it transmits the right data.");
    // Paint the frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Verify terminal buffer contains pattern.");
    // Verify the terminal side.
    verifyBuffer(termTb);

    Log::Comment(L"!!! OK. Set up the scroll region and let's get scrolling!");
    // This is a simulation of what TMUX does to scroll everything except the mode line.
    // First build up our VT strings...
    std::wstring reducedScrollRegion;
    {
        std::wstringstream wss;
        // For 20 tall buffer...
        // ESC[1;19r
        // Set scroll region to lines 1-19.
        wss << L"\x1b[1;" << initialTermView.Height() - 1 << L"r";
        reducedScrollRegion = wss.str();
    }
    std::wstring completeScrollRegion;
    {
        std::wstringstream wss;
        // For 20 tall buffer...
        // ESC[1;20r
        // Set scroll region to lines 1-20. (or the whole buffer)
        wss << L"\x1b[1;" << initialTermView.Height() << L"r";
        completeScrollRegion = wss.str();
    }
    std::wstring reducedCursorBottomRight;
    {
        std::wstringstream wss;
        // For 20 tall and 100 wide buffer
        // ESC[19;100H
        // Put cursor on line 19 (1 before last) and the right most column 100.
        // (Remember that in VT, we start counting from 1 not 0.)
        wss << L"\x1b[" << initialTermView.Height() - 1 << L";" << initialTermView.Width() << "H";
        reducedCursorBottomRight = wss.str();
    }
    std::wstring completeCursorAtPromptLine;
    {
        std::wstringstream wss;
        // For 20 tall and 100 wide buffer
        // ESC[19;1H
        // Put cursor on line 19 (1 before last) and the left most column 1.
        // (Remember that in VT, we start counting from 1 not 0.)
        wss << L"\x1b[" << initialTermView.Height() - 1 << L";1H";
        completeCursorAtPromptLine = wss.str();
    }

    Log::Comment(L"Perform all the operations on the buffer.");

    // OK this is what TMUX does.
    // 1. Mark off the scroll area as everything but the mode line.
    hostSm.ProcessString(reducedScrollRegion);
    // 2. Put the cursor in the bottom-right corner of the scroll region.
    hostSm.ProcessString(reducedCursorBottomRight);
    // 3. Send a single newline which should do the heavy lifting
    //    of pushing everything in the scroll region up by 1 line and
    //    leave everything outside the region alone.

    // This entire block is subject to change in the future with optimizations.
    {
        // Cursor gets redrawn in the bottom right of the scroll region with the repaint that is forced
        // early while the screen is rotated.
        std::stringstream ss;
        ss << "\x1b[" << initialTermView.Height() - 1 << ";" << initialTermView.Width() << "H";
        expectedOutput.push_back(ss.str());

        expectedOutput.push_back("\x1b[?25h"); // turn the cursor back on too.
    }

    hostSm.ProcessString(L"\n");
    // 4. Remove the scroll area by setting it to the entire size of the viewport.
    hostSm.ProcessString(completeScrollRegion);
    // 5. Put the cursor back at the beginning of the new line that was just revealed.
    hostSm.ProcessString(completeCursorAtPromptLine);

    // Set up the verifications like above.
    auto verifyBufferAfter = [&](const TextBuffer& tb) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor is waiting on the freshly revealed line (1 above mode line)
        // and in the left most column.
        VERIFY_ARE_EQUAL(initialTermView.Height() - 2, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(0, cursor.GetPosition().X);

        // For all rows except the last two, verify that we have a run of four letters.
        for (auto i = 0; i < rowsToWrite - 1; ++i)
        {
            // Start with B this time because the A line got scrolled off the top.
            const std::wstring expectedString(4, static_cast<wchar_t>(L'B' + i));
            const COORD expectedPos{ 0, gsl::narrow<SHORT>(i) };
            TestUtils::VerifyExpectedString(tb, expectedString, expectedPos);
        }

        // For the second to last row, verify that it is blank.
        {
            const std::wstring expectedBlankLine(initialTermView.Width(), L' ');
            const COORD blankLinePos{ 0, gsl::narrow<SHORT>(rowsToWrite - 1) };
            TestUtils::VerifyExpectedString(tb, expectedBlankLine, blankLinePos);
        }

        // For the last row, verify we have an entire row of asterisks for the mode line.
        {
            const std::wstring expectedModeLine(initialTermView.Width() - 1, L'*');
            const COORD modeLinePos{ 0, gsl::narrow<SHORT>(rowsToWrite) };
            TestUtils::VerifyExpectedString(tb, expectedModeLine, modeLinePos);
        }
    };

    // This will verify the text emitted from the PTY.

    expectedOutput.push_back("\x1b[H"); // cursor returns to top left corner.
    for (auto i = 0; i < rowsToWrite - 1; ++i)
    {
        const std::string expectedString(4, static_cast<char>('B' + i));
        expectedOutput.push_back(expectedString);
        expectedOutput.push_back("\x1b[K"); // erase the rest of the line.
        expectedOutput.push_back("\r\n");
    }
    {
        expectedOutput.push_back(""); // nothing for the empty line
        expectedOutput.push_back("\x1b[K"); // erase the rest of the line.
        expectedOutput.push_back("\r\n");
    }
    {
        const std::string expectedString(initialTermView.Width() - 1, '*');
        // There will be one extra blank space at the end of the line, to prevent delayed EOL wrapping
        expectedOutput.push_back(expectedString + " ");
    }
    {
        // Cursor gets reset into second line from bottom, left most column
        std::stringstream ss;
        ss << "\x1b[" << initialTermView.Height() - 1 << ";1H";
        expectedOutput.push_back(ss.str());
    }
    expectedOutput.push_back("\x1b[?25h"); // turn the cursor back on too.

    Log::Comment(L"Verify host buffer contains pattern moved up one and mode line still in place.");
    // Verify the host side.
    verifyBufferAfter(hostTb);

    Log::Comment(L"Emit PTY frame and validate it transmits the right data.");
    // Paint the frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Verify terminal buffer contains pattern moved up one and mode line still in place.");
    // Verify the terminal side.
    verifyBufferAfter(termTb);
}

void ConptyRoundtripTests::DontWrapMoveCursorInSingleFrame()
{
    // See https://github.com/microsoft/terminal/pull/5181#issuecomment-607427840
    Log::Comment(L"This is a test for when a line of text exactly wrapped, but "
                 L"the cursor didn't end the frame at the end of line (waiting "
                 L"for more wrapped text). We should still move the cursor in "
                 L"this case.");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    auto verifyBuffer = [](const TextBuffer& tb) {
        // Simple verification: Make sure the cursor is in the correct place,
        // and that it's visible. We don't care so much about the buffer
        // contents in this test.
        const COORD expectedCursor{ 8, 3 };
        VERIFY_ARE_EQUAL(expectedCursor, tb.GetCursor().GetPosition());
        VERIFY_IS_TRUE(tb.GetCursor().IsVisible());
    };

    hostSm.ProcessString(L"\x1b[?25l");
    hostSm.ProcessString(L"\x1b[H");
    hostSm.ProcessString(L"\x1b[75C");
    hostSm.ProcessString(L"XXXXX");
    hostSm.ProcessString(L"\x1b[4;9H");
    hostSm.ProcessString(L"\x1b[?25h");

    Log::Comment(L"Checking the host buffer state");
    verifyBuffer(hostTb);

    expectedOutput.push_back("\x1b[75C");
    expectedOutput.push_back("XXXXX");
    expectedOutput.push_back("\x1b[4;9H");
    // We're _not_ expecting a cursor on here, because we didn't actually hide
    // the cursor during the course of this frame

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Checking the terminal buffer state");
    verifyBuffer(termTb);
}

void ConptyRoundtripTests::ClearHostTrickeryTest()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:paintEachNewline", L"{0, 1, 2}")
        TEST_METHOD_PROPERTY(L"Data:cursorOnNextLine", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:paintAfterDECALN", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:changeAttributes", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:useLongSpaces", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:printTextAfterSpaces", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();
    constexpr int PaintEveryNewline = 0;
    constexpr int PaintAfterAllNewlines = 1;
    constexpr int DontPaintAfterNewlines = 2;

    INIT_TEST_PROPERTY(int, paintEachNewline, L"Any of: manually PaintFrame after each newline is emitted, once at the end of all newlines, or not at all");
    INIT_TEST_PROPERTY(bool, cursorOnNextLine, L"Either leave the cursor on the first line, or place it on the second line of the buffer");
    INIT_TEST_PROPERTY(bool, paintAfterDECALN, L"Controls whether we manually paint a frame after the DECALN sequence is emitted.");
    INIT_TEST_PROPERTY(bool, changeAttributes, L"If true, change the text attributes after the 'A's and spaces");
    INIT_TEST_PROPERTY(bool, useLongSpaces, L"If true, print 10 spaces instead of 5, longer than a CUF sequence.");
    INIT_TEST_PROPERTY(bool, printTextAfterSpaces, L"If true, print \"ZZZZZ\" after the spaces on the first line.");

    // See https://github.com/microsoft/terminal/issues/5039#issuecomment-606833841
    Log::Comment(L"This is a more than comprehensive test for GH#5039. We're "
                 L"going to print some text to the buffer, then fill the alt-"
                 L"buffer with text, then switch back to the main buffer. The "
                 L"text from the alt buffer should not pollute the main buffer.");

    // The text we're printing will look like one of the following, with the
    // cursor on the _
    //  * cursorOnNextLine=false, useLongSpaces=false:
    //    AAAAA     ZZZZZ_
    //  * cursorOnNextLine=false, useLongSpaces=true:
    //    AAAAA          ZZZZZ_
    //  * cursorOnNextLine=true, useLongSpaces=false:
    //    AAAAA     ZZZZZ
    //    BBBBB_
    //  * cursorOnNextLine=true, useLongSpaces=true:
    //    AAAAA          ZZZZZ
    //    BBBBB_
    //
    // If printTextAfterSpaces=false, then we won't print the "ZZZZZ"
    //
    // The interesting case that repros the bug in GH#5039 is
    //  - paintEachNewline=DontPaintAfterNewlines (2)
    //  - cursorOnNextLine=false
    //  - paintAfterDECALN=<any>
    //  - changeAttributes=true
    //  - useLongSpaces=<any>
    //  - printTextAfterSpaces=<any>
    //
    // All the possible cases are left here though, to catch potential future regressions.
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    auto verifyBuffer = [&cursorOnNextLine, &useLongSpaces, &printTextAfterSpaces](const TextBuffer& tb,
                                                                                   const til::rectangle viewport) {
        // We _would_ expect the Terminal's cursor to be on { 8, 0 }, but this
        // is currently broken due to #381/#4676. So we'll use the viewport
        // provided to find the actual Y position of the cursor.
        const short viewTop = viewport.origin().y<short>();
        const short cursorRow = viewTop + (cursorOnNextLine ? 1 : 0);
        const short cursorCol = (cursorOnNextLine ? 5 :
                                                    (10 + (useLongSpaces ? 5 : 0) + (printTextAfterSpaces ? 5 : 0)));
        const COORD expectedCursor{ cursorCol, cursorRow };

        VERIFY_ARE_EQUAL(expectedCursor, tb.GetCursor().GetPosition());
        VERIFY_IS_TRUE(tb.GetCursor().IsVisible());
        auto iter = TestUtils::VerifyExpectedString(tb, L"AAAAA", { 0, viewTop });
        TestUtils::VerifyExpectedString(useLongSpaces ? L"          " : L"     ", iter);
        if (printTextAfterSpaces)
        {
            TestUtils::VerifyExpectedString(L"ZZZZZ", iter);
        }
        else
        {
            TestUtils::VerifyExpectedString(L"     ", iter);
        }
        TestUtils::VerifyExpectedString(L"     ", iter);

        if (cursorOnNextLine)
        {
            TestUtils::VerifyExpectedString(tb, L"BBBBB", { 0, cursorRow });
        }
    };

    // We're _not_ checking the conpty output during this test, only the side effects.
    _checkConptyOutput = false;

    gci.LockConsole(); // Lock must be taken to manipulate alt/main buffer state.
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    Log::Comment(L"Setting up the host buffer...");
    hostSm.ProcessString(L"AAAAA");
    hostSm.ProcessString(useLongSpaces ? L"          " : L"     ");
    if (changeAttributes)
    {
        hostSm.ProcessString(L"\x1b[44m");
    }
    if (printTextAfterSpaces)
    {
        hostSm.ProcessString(L"ZZZZZ");
    }
    hostSm.ProcessString(L"\x1b[0m");

    if (cursorOnNextLine)
    {
        hostSm.ProcessString(L"\n");
        hostSm.ProcessString(L"BBBBB");
    }
    Log::Comment(L"Painting after the initial setup.");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Switching to the alt buffer and using DECALN to fill it with 'E's");
    hostSm.ProcessString(L"\x1b[?1049h");
    hostSm.ProcessString(L"\x1b#8");
    if (paintAfterDECALN)
    {
        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    for (auto i = 0; i < si.GetViewport().Height(); i++)
    {
        hostSm.ProcessString(L"\n");
        if (paintEachNewline == PaintEveryNewline)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }
    }
    if (paintEachNewline == PaintAfterAllNewlines)
    {
        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }
    Log::Comment(L"Returning to the main buffer.");
    hostSm.ProcessString(L"\x1b[?1049l");

    Log::Comment(L"Checking the host buffer state");
    verifyBuffer(hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Checking the terminal buffer state");
    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::OverstrikeAtBottomOfBuffer()
{
    // See https://github.com/microsoft/terminal/pull/5181#issuecomment-607545241
    Log::Comment(L"This test replicates the zsh menu-complete functionality. In"
                 L" the course of a single frame, we're going to both scroll "
                 L"the frame and print multiple lines of text above the bottom line.");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    auto verifyBuffer = [](const TextBuffer& tb,
                           const til::rectangle viewport) {
        const auto lastRow = viewport.bottom<short>() - 1;
        const til::point expectedCursor{ 0, lastRow - 1 };
        VERIFY_ARE_EQUAL(expectedCursor, til::point{ tb.GetCursor().GetPosition() });
        VERIFY_IS_TRUE(tb.GetCursor().IsVisible());

        TestUtils::VerifyExpectedString(tb, L"AAAAAAAAAA          DDDDDDDDDD", til::point{ 0, lastRow - 2 });
        TestUtils::VerifyExpectedString(tb, L"BBBBBBBBBB", til::point{ 0, lastRow - 1 });
        TestUtils::VerifyExpectedString(tb, L"FFFFFFFFFE", til::point{ 0, lastRow });
    };

    _logConpty = true;
    // We're _not_ checking the conpty output during this test, only the side effects.
    _checkConptyOutput = false;

    hostSm.ProcessString(L"\x1b#8");

    hostSm.ProcessString(L"\x1b[32;1H");

    hostSm.ProcessString(L"\x1b[J");
    hostSm.ProcessString(L"AAAAAAAAAA");
    hostSm.ProcessString(L"\x1b[K");
    hostSm.ProcessString(L"\r");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"BBBBBBBBBB");
    hostSm.ProcessString(L"\x1b[K");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"CCCCCCCCCC");
    hostSm.ProcessString(L"\x1b[2A");
    hostSm.ProcessString(L"\r");
    hostSm.ProcessString(L"\x1b[20C");
    hostSm.ProcessString(L"DDDDDDDDDD");
    hostSm.ProcessString(L"\x1b[K");
    hostSm.ProcessString(L"\r");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"\x1b[1B");
    hostSm.ProcessString(L"EEEEEEEEEE");
    hostSm.ProcessString(L"\r");
    hostSm.ProcessString(L"FFFFFFFFF");
    hostSm.ProcessString(L"\r");
    hostSm.ProcessString(L"\x1b[A");
    hostSm.ProcessString(L"\x1b[A");
    hostSm.ProcessString(L"\n");

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");

    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::MarginsWithStatusLine()
{
    // See https://github.com/microsoft/terminal/issues/5161
    //
    // This test reproduces a case from the MSYS/cygwin (runtime < 3.1) vim.
    // From what I can tell, they implement scrolling by emitting a newline at
    // the bottom of the buffer (to create a new blank line), then they use
    // ScrollConsoleScreenBuffer to shift the status line(s) down a line, and
    // then they re-printing the status line.
    Log::Comment(L"Newline, and scroll the bottom lines of the buffer down with"
                 L" ScrollConsoleScreenBuffer to emulate how cygwin VIM works");
    VERIFY_IS_NOT_NULL(_pVtRenderEngine.get());

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    auto verifyBuffer = [](const TextBuffer& tb,
                           const til::rectangle viewport) {
        const auto lastRow = viewport.bottom<short>() - 1;
        const til::point expectedCursor{ 1, lastRow };
        VERIFY_ARE_EQUAL(expectedCursor, til::point{ tb.GetCursor().GetPosition() });
        VERIFY_IS_TRUE(tb.GetCursor().IsVisible());

        TestUtils::VerifyExpectedString(tb, L"EEEEEEEEEE", til::point{ 0, lastRow - 4 });
        TestUtils::VerifyExpectedString(tb, L"AAAAAAAAAA", til::point{ 0, lastRow - 3 });
        TestUtils::VerifyExpectedString(tb, L"          ", til::point{ 0, lastRow - 2 });
        TestUtils::VerifyExpectedString(tb, L"XBBBBBBBBB", til::point{ 0, lastRow - 1 });
        TestUtils::VerifyExpectedString(tb, L"YCCCCCCCCC", til::point{ 0, lastRow });
    };

    // We're _not_ checking the conpty output during this test, only the side effects.
    _checkConptyOutput = false;

    // Use DECALN to fill the buffer with 'E's.
    hostSm.ProcessString(L"\x1b#8");

    const short originalBottom = si.GetViewport().BottomInclusive();
    // Print 3 lines into the bottom of the buffer:
    // AAAAAAAAAA
    // BBBBBBBBBB
    // CCCCCCCCCC
    // In this test, the 'B' and 'C' lines represent the status lines at the
    // bottom of vim, and the 'A' line is a buffer line.
    hostSm.ProcessString(L"\x1b[30;1H");

    hostSm.ProcessString(L"AAAAAAAAAA");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"BBBBBBBBBB");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"CCCCCCCCCC");

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // After printing the 'C' line, the cursor is on the bottom line of the viewport.
    // Emit a newline here to get a new line at the bottom of the viewport.
    hostSm.ProcessString(L"\n");
    const short newBottom = si.GetViewport().BottomInclusive();

    {
        // Emulate calling ScrollConsoleScreenBuffer to scroll the B and C lines
        // down one line.
        SMALL_RECT src;
        src.Top = newBottom - 2;
        src.Left = 0;
        src.Right = si.GetViewport().Width();
        src.Bottom = originalBottom;
        COORD tgt = { 0, newBottom - 1 };
        TextAttribute useThisAttr(0x07); // We don't terribly care about the attributes so this is arbitrary
        ScrollRegion(si, src, std::nullopt, tgt, L' ', useThisAttr);
    }

    // Move the cursor to the location of the B line
    hostSm.ProcessString(L"\x1b[31;1H");

    // Print an 'X' on the 'B' line, and a 'Y' on the 'C' line.
    hostSm.ProcessString(L"X");
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(L"Y");

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");

    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
}
