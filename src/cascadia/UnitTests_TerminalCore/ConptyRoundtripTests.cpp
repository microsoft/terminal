// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This test class creates an in-proc conpty host as well as a Terminal, to
// validate that strings written to the conpty create the same response on the
// terminal end. Tests can be written that validate both the contents of the
// host buffer as well as the terminal buffer. Everytime that
// `renderer.PaintFrame()` is called, the tests will validate the expected
// output, and then flush the output of the VtEngine straight to the Terminal.

#include "pch.h"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/convert.hpp"

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"
#include "../../renderer/vt/XtermEngine.hpp"

class InputBuffer; // This for some reason needs to be fwd-decl'd
#include "../host/inputBuffer.hpp"
#include "../host/readDataCooked.hpp"
#include "../host/output.h"
#include "../host/_stream.h" // For WriteCharsLegacy
#include "../host/cmdline.h" // For WC_LIMIT_BACKSPACE
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

class TerminalCoreUnitTests::ConptyRoundtripTests final
{
    // !!! DANGER: Many tests in this class expect the Terminal and Host buffers
    // to be 80x32. If you change these, you'll probably inadvertently break a
    // bunch of tests !!!
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

        auto vtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hFile),
                                                               initialViewport);
        auto pfn = std::bind(&ConptyRoundtripTests::_writeCallback, this, std::placeholders::_1, std::placeholders::_2);
        vtRenderEngine->SetTestCallback(pfn);

        // Enable the resize quirk, as the Terminal is going to be reacting as if it's enabled.
        vtRenderEngine->SetResizeQuirk(true);

        // Configure the OutputStateMachine's _pfnFlushToTerminal
        // Use OutputStateMachineEngine::SetTerminalConnection
        g.pRender->AddRenderEngine(vtRenderEngine.get());
        gci.GetActiveOutputBuffer().SetTerminalConnection(vtRenderEngine.get());

        _pConApi = std::make_unique<ConhostInternalGetSet>(gci);

        // Manually set the console into conpty mode. We're not actually going
        // to set up the pipes for conpty, but we want the console to behave
        // like it would in conpty mode.
        g.EnableConptyModeForTests(std::move(vtRenderEngine));

        expectedOutput.clear();
        _checkConptyOutput = true;
        _logConpty = false;

        VERIFY_ARE_EQUAL(gci.GetActiveOutputBuffer().GetViewport().Dimensions(),
                         gci.GetActiveOutputBuffer().GetBufferSize().Dimensions(),
                         L"If this test fails, then there's a good chance "
                         L"another test resized the buffer but didn't use IsolationLevel:Method");
        VERIFY_ARE_EQUAL(gci.GetActiveOutputBuffer().GetViewport(),
                         gci.GetActiveOutputBuffer().GetBufferSize(),
                         L"If this test fails, then there's a good chance "
                         L"another test resized the buffer but didn't use IsolationLevel:Method");

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

    TEST_METHOD(PassthroughClearAll);

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
    TEST_METHOD(OutputWrappedLineWithSpace);
    TEST_METHOD(OutputWrappedLineWithSpaceAtBottomOfBuffer);

    TEST_METHOD(BreakLinesOnCursorMovement);

    TEST_METHOD(ScrollWithMargins);

    TEST_METHOD(TestCursorInDeferredEOLPositionOnNewLineWithSpaces);

    TEST_METHOD(ResizeRepaintVimExeBuffer);

    TEST_METHOD(ClsAndClearHostClearsScrollbackTest);

    TEST_METHOD(TestResizeWithCookedRead);

    TEST_METHOD(NewLinesAtBottomWithBackground);

    TEST_METHOD(WrapNewLineAtBottom);
    TEST_METHOD(WrapNewLineAtBottomLikeMSYS);

    TEST_METHOD(DeleteWrappedWord);

    TEST_METHOD(HyperlinkIdConsistency);

    TEST_METHOD(ResizeInitializeBufferWithDefaultAttrs);

private:
    bool _writeCallback(const char* const pch, size_t const cch);
    void _flushFirstFrame();
    void _resizeConpty(const unsigned short sx, const unsigned short sy);

    [[nodiscard]] std::tuple<TextBuffer*, TextBuffer*> _performResize(const til::size& newSize);

    std::deque<std::string> expectedOutput;

    std::unique_ptr<CommonState> m_state;
    std::unique_ptr<Microsoft::Console::VirtualTerminal::ConGetSet> _pConApi;

    // Tests can set these variables how they link to configure the behavior of the test harness.
    bool _checkConptyOutput{ true }; // If true, the test class will check that the output from conpty was expected
    bool _logConpty{ false }; // If true, the test class will log all the output from conpty. Helpful for debugging.

    DummyRenderTarget emptyRT;
    std::unique_ptr<Terminal> term;

    ApiRoutines _apiRoutines;
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
        auto& g = ServiceLocator::LocateGlobals();
        auto& gci = g.getConsoleInformation();
        VERIFY_SUCCEEDED(gci.GetVtIo()->SuppressResizeRepaint());
    }
}

[[nodiscard]] std::tuple<TextBuffer*, TextBuffer*> ConptyRoundtripTests::_performResize(const til::size& newSize)
{
    // IMPORTANT! Anyone calling this should make sure that the test is running
    // in IsolationLevel: Method. If you don't add that, then it might secretly
    // pollute other tests!

    Log::Comment(L"========== Resize the Terminal and conpty ==========");

    auto resizeResult = term->UserResize(newSize);
    VERIFY_SUCCEEDED(resizeResult);
    _resizeConpty(newSize.width<unsigned short>(), newSize.height<unsigned short>());

    // After we resize, make sure to get the new textBuffers
    return { &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetTextBuffer(),
             term->_buffer.get() };
}

void ConptyRoundtripTests::ConptyOutputTestCanary()
{
    Log::Comment(NoThrowString().Format(
        L"This is a simple test to make sure that everything is working as expected."));

    _flushFirstFrame();
}

void ConptyRoundtripTests::SimpleWriteOutputTest()
{
    Log::Comment(NoThrowString().Format(
        L"Write some simple output, and make sure it gets rendered largely "
        L"unmodified to the terminal"));

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
        VERIFY_IS_TRUE(row0.WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.WasWrapForced());

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
        VERIFY_IS_TRUE(row0.WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.WasWrapForced());

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
        VERIFY_IS_FALSE(row0.WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.WasWrapForced());

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
        VERIFY_IS_FALSE(row0.WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.WasWrapForced());

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

    const til::size newViewportSize{ TerminalViewWidth + dx,
                                     TerminalViewHeight + dy };
    // After we resize, make sure to get the new textBuffers
    std::tie(hostTb, termTb) = _performResize(newViewportSize);

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto thirdHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, thirdHostView.Top());
    VERIFY_ARE_EQUAL(newViewportSize.height(), thirdHostView.BottomExclusive());

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
    VERIFY_ARE_EQUAL(newViewportSize.height(), fourthHostView.BottomExclusive());

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

void ConptyRoundtripTests::PassthroughClearAll()
{
    // see https://github.com/microsoft/terminal/issues/2832
    Log::Comment(L"This is a test to make sure that when the client emits a "
                 L"^[[2J, we actually forward the 2J to the terminal, to move "
                 L"the viewport. 2J importantly moves the viewport, so that "
                 L"all the _current_ buffer contents are moved to scrollback. "
                 L"We shouldn't just paint over the current viewport with spaces.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    auto& sm = si.GetStateMachine();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        if (i > 0)
        {
            sm.ProcessString(L"\r\n");
        }

        sm.ProcessString(L"~");
    }

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport, const bool afterClear = false) {
        const auto firstRow = viewport.top<short>();
        const auto width = viewport.width<short>();

        // "~" rows
        for (short row = 0; row < viewport.bottom<short>(); row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));
            VERIFY_IS_FALSE(tb.GetRowByOffset(row).WasWrapForced());
            auto iter = tb.GetCellDataAt({ 0, row });
            if (afterClear && row >= viewport.top<short>())
            {
                TestUtils::VerifySpanOfText(L" ", iter, 0, width);
            }
            else
            {
                TestUtils::VerifySpanOfText(L"~", iter, 0, 1);
                TestUtils::VerifySpanOfText(L" ", iter, 0, width - 1);
            }
        }
    };

    Log::Comment(L"========== Checking the host buffer state (before) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (before) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());

    const til::rectangle originalTerminalView{ term->_mutableViewport.ToInclusive() };

    // Here, we'll emit the 2J to EraseAll, and move the viewport contents into
    // the scrollback.
    sm.ProcessString(L"\x1b[2J");

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Make sure that the terminal's new viewport is actually just lower than it
    // used to be.
    const til::rectangle newTerminalView{ term->_mutableViewport.ToInclusive() };
    VERIFY_ARE_EQUAL(end, newTerminalView.top<short>());
    VERIFY_IS_GREATER_THAN(newTerminalView.top(), originalTerminalView.top());

    Log::Comment(L"========== Checking the host buffer state (after) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), true);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());
    Log::Comment(L"========== Checking the terminal buffer state (after) ==========");
    verifyBuffer(*termTb, newTerminalView, true);
}

void ConptyRoundtripTests::PassthroughHardReset()
{
    // This test is highly similar to PassthroughClearScrollback.
    Log::Comment(NoThrowString().Format(
        L"Write more lines of output than there are lines in the viewport. Clear everything with ^[c"));

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
        // Buffer contents should look like the following: (80 wide)
        // (w) means we hard wrapped the line
        // (b) means the line is _not_ wrapped (it's broken, the default state.)
        // cursor is on the '_'
        //
        // |AAAAAAAA...AAAA| (w)
        // |AAAAA_  ...    | (b) (There are 20 'A's on this line.)
        // |        ...    | (b)

        VERIFY_IS_TRUE(tb.GetRowByOffset(0).WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(1).WasWrapForced());
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

    // The following diagrams show the buffer contents after each string emitted
    // from conpty. For each of these diagrams:
    // (w) means we hard wrapped the line
    // (b) means the line is _not_ wrapped (it's broken, the default state.)
    // cursor is on the '_'

    // Initial state:
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |_              | (b)

    expectedOutput.push_back(std::string(TerminalViewWidth, 'A'));
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AAAA|_ (w) The cursor is actually on the last A here

    // TODO GH#5228 might break the "newline & repaint the wrapped char" checks here, that's okay.
    expectedOutput.push_back("\r"); // This \r\n is emitted by ScrollFrame to
    expectedOutput.push_back("\n"); // add a newline to the bottom of the buffer
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AAAA| (b)
    // |_              | (b)

    expectedOutput.push_back("\x1b[31;80H"); // Move the cursor BACK to the wrapped row
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AAAA| (b) The cursor is actually on the last A here
    // |               | (b)

    expectedOutput.push_back(std::string(1, 'A')); // Reprint the last character of the wrapped row
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AAAA|_ (w) The cursor is actually on the last A here
    // |               | (b)

    expectedOutput.push_back(std::string(20, 'A')); // Print the second line.
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AAAA| (w)
    // |AAAAA_         | (b) There are 20 'A's on this line.

    hostSm.ProcessString(std::wstring(wrappedLineLength, L'A'));

    auto verifyBuffer = [](const TextBuffer& tb, const short wrappedRow) {
        // Buffer contents should look like the following: (80 wide)
        // (w) means we hard wrapped the line
        // (b) means the line is _not_ wrapped (it's broken, the default state.)
        // cursor is on the '_'
        //
        // |X              | (b)
        // |X              | (b)
        // ...
        // |X              | (b)
        // |AAAAAAAA...AAAA| (w)
        // |AAAAA_  ...    | (b) (There are 20 'A's on this line.)

        VERIFY_IS_TRUE(tb.GetRowByOffset(wrappedRow).WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(wrappedRow + 1).WasWrapForced());

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

        VERIFY_IS_TRUE(tb.GetRowByOffset(wrappedRow).WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(wrappedRow + 1).WasWrapForced());

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

void ConptyRoundtripTests::OutputWrappedLineWithSpace()
{
    // See https://github.com/microsoft/terminal/pull/5181#issuecomment-610110348
    Log::Comment(L"Ensures that a buffer line in conhost that wrapped _on a "
                 L"space_ will still be emitted as wrapped.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    const auto firstTextLength = TerminalViewWidth - 2;
    const auto spacesLength = 3;
    const auto secondTextLength = 1;

    sm.ProcessString(std::wstring(firstTextLength, L'A'));
    sm.ProcessString(std::wstring(spacesLength, L' '));
    sm.ProcessString(std::wstring(secondTextLength, L'B'));

    auto verifyBuffer = [&](const TextBuffer& tb) {
        // Buffer contents should look like the following: (80 wide)
        // (w) means we hard wrapped the line
        // (b) means the line is _not_ wrapped (it's broken, the default state.)
        //
        // |AAAA...AA  | (w)
        // | B_ ...    | (b) (cursor is on the '_')
        // |    ...    | (b)

        VERIFY_IS_TRUE(tb.GetRowByOffset(0).WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(1).WasWrapForced());

        // First row
        auto iter0 = tb.GetCellDataAt({ 0, 0 });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, firstTextLength);
        TestUtils::VerifySpanOfText(L" ", iter0, 0, 2);

        // Second row
        auto iter1 = tb.GetCellDataAt({ 0, 1 });
        TestUtils::VerifySpanOfText(L" ", iter1, 0, 1);
        auto iter2 = tb.GetCellDataAt({ 1, 1 });
        TestUtils::VerifySpanOfText(L"B", iter2, 0, secondTextLength);
    };

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(hostTb);

    std::string firstLine = std::string(firstTextLength, 'A');
    firstLine += "  ";
    std::string secondLine{ " B" };

    expectedOutput.push_back(firstLine);
    expectedOutput.push_back(secondLine);
    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");
    verifyBuffer(termTb);
}

void ConptyRoundtripTests::OutputWrappedLineWithSpaceAtBottomOfBuffer()
{
    // See https://github.com/microsoft/terminal/pull/5181#issuecomment-610110348
    // This is the same test as OutputWrappedLineWithSpace, but at the bottom of
    // the buffer, so we get scrolling behavior as well.
    Log::Comment(L"Ensures that a buffer line in conhost that wrapped _on a "
                 L"space_ will still be emitted as wrapped.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
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

        sm.ProcessString(L"X\n");

        VERIFY_SUCCEEDED(renderer.PaintFrame());
    }

    const auto firstTextLength = TerminalViewWidth - 2;
    const auto spacesLength = 3;
    const auto secondTextLength = 1;

    std::string firstLine = std::string(firstTextLength, 'A');
    firstLine += "  ";
    std::string secondLine{ " B" };

    // The following diagrams show the buffer contents after each string emitted
    // from conpty. For each of these diagrams:
    // (w) means we hard wrapped the line
    // (b) means the line is _not_ wrapped (it's broken, the default state.)
    // cursor is on the '_'

    // Initial state:
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |_              | (b)

    expectedOutput.push_back(firstLine);
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AA _| (w) The cursor is actually on the last ' ' here

    // TODO GH#5228 might break the "newline & repaint the wrapped char" checks here, that's okay.
    expectedOutput.push_back("\r"); // This \r\n is emitted by ScrollFrame to
    expectedOutput.push_back("\n"); // add a newline to the bottom of the buffer
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AA | (b)
    // |_              | (b)

    expectedOutput.push_back("\x1b[31;80H"); // Move the cursor BACK to the wrapped row
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AA _| (b) The cursor is actually on the last ' ' here
    // |               | (b)

    expectedOutput.push_back(std::string(1, ' ')); // Reprint the last character of the wrapped row
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AA  |_ (w) The cursor is actually on the last ' ' here
    // |               | (b)

    expectedOutput.push_back(secondLine);
    // |X              | (b)
    // |X              | (b)
    // ...
    // |X              | (b)
    // |AAAAAAAA...AA  | (w)
    // | B_            | (b)

    sm.ProcessString(std::wstring(firstTextLength, L'A'));
    sm.ProcessString(std::wstring(spacesLength, L' '));
    sm.ProcessString(std::wstring(secondTextLength, L'B'));

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport) {
        // Buffer contents should look like the following: (80 wide)
        // (w) means we hard wrapped the line
        // (b) means the line is _not_ wrapped (it's broken, the default state.)
        //
        // |AAAA...AA  | (w)
        // | B_ ...    | (b) (cursor is on the '_')
        // |    ...    | (b)

        const short wrappedRow = viewport.bottom<short>() - 2;
        VERIFY_IS_TRUE(tb.GetRowByOffset(wrappedRow).WasWrapForced());
        VERIFY_IS_FALSE(tb.GetRowByOffset(wrappedRow + 1).WasWrapForced());

        // First row
        auto iter0 = tb.GetCellDataAt({ 0, wrappedRow });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, firstTextLength);
        TestUtils::VerifySpanOfText(L" ", iter0, 0, 2);

        // Second row
        auto iter1 = tb.GetCellDataAt({ 0, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L" ", iter1, 0, 1);
        auto iter2 = tb.GetCellDataAt({ 1, wrappedRow + 1 });
        TestUtils::VerifySpanOfText(L"B", iter2, 0, secondTextLength);
    };

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(hostTb, hostView.ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");
    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::BreakLinesOnCursorMovement()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:cursorMovementMode", L"{0, 1, 2, 3, 4, 5, 6}")
    END_TEST_METHOD_PROPERTIES();
    constexpr int MoveCursorWithCUP = 0;
    constexpr int MoveCursorWithCR_LF = 1;
    constexpr int MoveCursorWithLF_CR = 2;
    constexpr int MoveCursorWithVPR_CR = 3;
    constexpr int MoveCursorWithCUB_LF = 4;
    constexpr int MoveCursorWithCUD_CR = 5;
    constexpr int MoveCursorWithNothing = 6;
    INIT_TEST_PROPERTY(int, cursorMovementMode, L"Controls how we move the cursor, either with CUP, newline/carriage-return, or some other VT sequence");

    Log::Comment(L"This is a test for GH#5291. WSL vim uses spaces to clear the"
                 L" ends of blank lines, not EL. This test ensures we emit text"
                 L" from conpty such that the terminal re-creates the state of"
                 L" the host, which includes wrapped lines of lots of spaces.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    // Any of the cursor movements that use a LF will actually hard break the
    // line - everything else will leave it marked as wrapped.
    const bool expectHardBreak = (cursorMovementMode == MoveCursorWithLF_CR) ||
                                 (cursorMovementMode == MoveCursorWithCR_LF) ||
                                 (cursorMovementMode == MoveCursorWithCUB_LF);

    auto verifyBuffer = [&](const TextBuffer& tb,
                            const til::rectangle viewport) {
        const auto lastRow = viewport.bottom<short>() - 1;
        const til::point expectedCursor{ 5, lastRow };
        VERIFY_ARE_EQUAL(expectedCursor, til::point{ tb.GetCursor().GetPosition() });
        VERIFY_IS_TRUE(tb.GetCursor().IsVisible());

        for (auto y = viewport.top<short>(); y < lastRow; y++)
        {
            // We're using CUP to move onto the status line _always_, so the
            // second-last row will always be marked as wrapped.
            const auto rowWrapped = (!expectHardBreak) || (y == lastRow - 1);
            VERIFY_ARE_EQUAL(rowWrapped, tb.GetRowByOffset(y).WasWrapForced());
            TestUtils::VerifyExpectedString(tb, L"~    ", til::point{ 0, y });
        }

        TestUtils::VerifyExpectedString(tb, L"AAAAA", til::point{ 0, lastRow });
    };

    // We're _not_ checking the conpty output during this test, only the side effects.
    _logConpty = true;
    _checkConptyOutput = false;

    // Lock must be taken to manipulate alt/main buffer state.
    gci.LockConsole();
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    // Use DECALN to fill the buffer with 'E's.
    hostSm.ProcessString(L"\x1b#8");

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Switching to the alt buffer");
    hostSm.ProcessString(L"\x1b[?1049h");
    auto restoreBuffer = wil::scope_exit([&] { hostSm.ProcessString(L"\x1b[?1049l"); });
    auto& altBuffer = gci.GetActiveOutputBuffer();
    auto& altTextBuffer = altBuffer.GetTextBuffer();

    // Go home and clear the screen.
    hostSm.ProcessString(L"\x1b[H");
    hostSm.ProcessString(L"\x1b[2J");

    // Write out lines of '~' followed by enough spaces to fill the line.
    hostSm.ProcessString(L"\x1b[94m");
    for (auto y = 0; y < altBuffer.GetViewport().BottomInclusive(); y++)
    {
        // Vim uses CUP to position the cursor on the first cell of each row, every row.
        if (cursorMovementMode == MoveCursorWithCUP)
        {
            std::wstringstream ss;
            ss << L"\x1b[";
            ss << y + 1;
            ss << L";1H";
            hostSm.ProcessString(ss.str());
        }
        // As an additional test, try breaking lines manually with \r\n
        else if (cursorMovementMode == MoveCursorWithCR_LF)
        {
            // Don't need to newline on the 0'th row
            if (y > 0)
            {
                hostSm.ProcessString(L"\r\n");
            }
        }
        // As an additional test, try breaking lines manually with \n\r
        else if (cursorMovementMode == MoveCursorWithLF_CR)
        {
            // Don't need to newline on the 0'th row
            if (y > 0)
            {
                hostSm.ProcessString(L"\n\r");
            }
        }
        // As an additional test, move the cursor down with VPR, then to the start of the line with CR
        else if (cursorMovementMode == MoveCursorWithVPR_CR)
        {
            // Don't need to newline on the 0'th row
            if (y > 0)
            {
                hostSm.ProcessString(L"\x1b[1e");
                hostSm.ProcessString(L"\r");
            }
        }
        // As an additional test, move the cursor back with CUB, then down with LF
        else if (cursorMovementMode == MoveCursorWithCUB_LF)
        {
            // Don't need to newline on the 0'th row
            if (y > 0)
            {
                hostSm.ProcessString(L"\x1b[80D");
                hostSm.ProcessString(L"\n");
            }
        }
        // As an additional test, move the cursor down with CUD, then to the start of the line with CR
        else if (cursorMovementMode == MoveCursorWithCUD_CR)
        {
            // Don't need to newline on the 0'th row
            if (y > 0)
            {
                hostSm.ProcessString(L"\x1b[B");
                hostSm.ProcessString(L"\r");
            }
        }
        // Win32 vim.exe will simply do _nothing_ in this scenario. It'll just
        // print the lines one after the other, without moving the cursor,
        // relying on us auto moving to the following line.
        else if (cursorMovementMode == MoveCursorWithNothing)
        {
        }

        // IMPORTANT! The way vim writes these blank lines is as '~' followed by
        // enough spaces to fill the line.
        // This bug (GH#5291 won't repro if you don't have the spaces).
        std::wstring line{ L"~" };
        line += std::wstring(79, L' ');
        hostSm.ProcessString(line);
    }

    // Print the "Status Line"
    hostSm.ProcessString(L"\x1b[32;1H");
    hostSm.ProcessString(L"\x1b[m");
    hostSm.ProcessString(L"AAAAA");

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(altTextBuffer, altBuffer.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");

    verifyBuffer(termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::TestCursorInDeferredEOLPositionOnNewLineWithSpaces()
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();

    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    const auto termView = term->GetViewport();

    _flushFirstFrame();
    _checkConptyOutput = false;

    // newline down to the bottom
    hostSm.ProcessString(std::wstring(gsl::narrow_cast<size_t>(TerminalViewHeight), L'\n'));
    // fill width-1 with "A", then add one space and another character..
    hostSm.ProcessString(std::wstring(gsl::narrow_cast<size_t>(TerminalViewWidth) - 1, L'A') + L" B");

    auto verifyBuffer = [&](const TextBuffer& tb, SHORT bottomRow) {
        // Buffer contents should look like the following: (80 wide)
        // (w) means we hard wrapped the line
        // (b) means the line is _not_ wrapped (it's broken, the default state.)
        // cursor is on the '_'
        //
        // | ^ ^ ^ ^ ^ ^ ^ | ( ) (entire buffer above us; contents do not matter)
        // |               | ( ) (entire buffer above us; contents do not matter)
        // |AAAAAAAA...AAA | (w) (79 'A's, one space)
        // |B_      ...    | (b) (There's only one 'B' on this line)

        til::point cursorPos{ tb.GetCursor().GetPosition() };
        // The cursor should be on the second char of the last line
        VERIFY_ARE_EQUAL((til::point{ 1, bottomRow }), cursorPos);

        const auto& secondToLastRow = tb.GetRowByOffset(bottomRow - 1);
        const auto& lastRow = tb.GetRowByOffset(bottomRow);
        VERIFY_IS_TRUE(secondToLastRow.WasWrapForced());
        VERIFY_IS_FALSE(lastRow.WasWrapForced());

        auto expectedStringSecondToLastRow{ std::wstring(gsl::narrow_cast<size_t>(tb.GetSize().Width()) - 1, L'A') + L" " };
        TestUtils::VerifyExpectedString(tb, expectedStringSecondToLastRow, { 0, bottomRow - 1 });
        TestUtils::VerifyExpectedString(tb, L"B", { 0, bottomRow });
    };

    const auto hostView = si.GetViewport();
    verifyBuffer(hostTb, hostView.BottomInclusive());

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    const auto newTermView = term->GetViewport();
    verifyBuffer(termTb, newTermView.BottomInclusive());
}

void ConptyRoundtripTests::ResizeRepaintVimExeBuffer()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD_PROPERTIES()

    // See https://github.com/microsoft/terminal/issues/5428
    Log::Comment(L"This test emulates what happens when you decrease the width "
                 L"of the window while running vim.exe.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    // This is a helper that will recreate the way that vim redraws the buffer.
    auto drawVim = [&sm, &si]() {
        const auto hostView = si.GetViewport();
        const auto width = hostView.Width();

        // Write:
        // * AAA to the first line
        // * BBB to the second line
        // * a bunch of lines with a "~" followed by spaces (just the way vim.exe likes to render)
        // * A status line with a bunch of "X"s and _a single space in the last cell_.
        sm.ProcessString(L"AAA");
        sm.ProcessString(L"\r\n");
        sm.ProcessString(L"BBB");
        sm.ProcessString(L"\r\n");

        const auto end = 2 * hostView.Height();
        for (auto i = 2; i < hostView.BottomInclusive(); i++)
        {
            // IMPORTANT! The way vim writes these blank lines is as '~' followed by
            // enough spaces to fill the line.
            std::wstring line{ L"~" };
            line += std::wstring(width - 1, L' ');
            sm.ProcessString(line);
        }
        sm.ProcessString(std::wstring(width - 1, L'X'));
        sm.ProcessString(L" ");

        // Move the cursor back home, as if it's on the first "A". The bug won't
        // repro without this!
        sm.ProcessString(L"\x1b[H");
    };

    drawVim();

    const auto firstTextLength = TerminalViewWidth - 2;
    const auto spacesLength = 3;
    const auto secondTextLength = 1;

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport) {
        const auto firstRow = viewport.top<short>();
        const auto width = viewport.width<short>();

        // First row
        VERIFY_IS_FALSE(tb.GetRowByOffset(firstRow).WasWrapForced());
        auto iter0 = tb.GetCellDataAt({ 0, firstRow });
        TestUtils::VerifySpanOfText(L"A", iter0, 0, 3);
        TestUtils::VerifySpanOfText(L" ", iter0, 0, width - 3);

        // Second row
        VERIFY_IS_FALSE(tb.GetRowByOffset(firstRow + 1).WasWrapForced());
        auto iter1 = tb.GetCellDataAt({ 0, firstRow + 1 });
        TestUtils::VerifySpanOfText(L"B", iter1, 0, 3);
        TestUtils::VerifySpanOfText(L" ", iter1, 0, width - 3);

        // "~" rows
        for (short row = firstRow + 2; row < viewport.bottom<short>() - 1; row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));
            VERIFY_IS_TRUE(tb.GetRowByOffset(row).WasWrapForced());
            auto iter = tb.GetCellDataAt({ 0, row });
            TestUtils::VerifySpanOfText(L"~", iter, 0, 1);
            TestUtils::VerifySpanOfText(L" ", iter, 0, width - 1);
        }

        // Last row
        {
            short row = viewport.bottom<short>() - 1;
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));
            VERIFY_IS_TRUE(tb.GetRowByOffset(row).WasWrapForced());
            auto iter = tb.GetCellDataAt({ 0, row });
            TestUtils::VerifySpanOfText(L"X", iter, 0, width - 1);
            TestUtils::VerifySpanOfText(L" ", iter, 0, 1);
        }
    };

    Log::Comment(L"========== Checking the host buffer state (before) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (before) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());

    // After we resize, make sure to get the new textBuffers
    std::tie(hostTb, termTb) = _performResize({ TerminalViewWidth - 1,
                                                TerminalViewHeight });

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"Re-painting vim");
    // vim will redraw itself when it notices the buffer size change.
    drawVim();

    Log::Comment(L"========== Checking the host buffer state (after) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (after) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::ClsAndClearHostClearsScrollbackTest()
{
    // See https://github.com/microsoft/terminal/issues/3126#issuecomment-620677742

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:clearBufferMethod", L"{0, 1, 2}")
    END_TEST_METHOD_PROPERTIES();
    constexpr int ClearLikeCls = 0;
    constexpr int ClearLikeClearHost = 1;
    constexpr int ClearWithVT = 2;
    INIT_TEST_PROPERTY(int, clearBufferMethod, L"Controls whether we clear the buffer like cmd or like powershell");

    Log::Comment(L"This test checks the shims for cmd.exe and powershell.exe. "
                 L"Their build in commands for clearing the console buffer "
                 L"should work to clear the terminal buffer, not just the "
                 L"terminal viewport.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    auto& sm = si.GetStateMachine();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    const auto hostView = si.GetViewport();
    const auto end = 2 * hostView.Height();
    for (auto i = 0; i < end; i++)
    {
        if (i > 0)
        {
            sm.ProcessString(L"\r\n");
        }

        sm.ProcessString(L"~");
    }

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport, const bool afterClear = false) {
        const auto firstRow = viewport.top<short>();
        const auto width = viewport.width<short>();

        // "~" rows
        for (short row = 0; row < viewport.bottom<short>(); row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));
            VERIFY_IS_FALSE(tb.GetRowByOffset(row).WasWrapForced());
            auto iter = tb.GetCellDataAt({ 0, row });
            if (afterClear)
            {
                TestUtils::VerifySpanOfText(L" ", iter, 0, width);
            }
            else
            {
                TestUtils::VerifySpanOfText(L"~", iter, 0, 1);
                TestUtils::VerifySpanOfText(L" ", iter, 0, width - 1);
            }
        }
    };

    Log::Comment(L"========== Checking the host buffer state (before) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (before) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());

    VERIFY_ARE_EQUAL(si.GetViewport().Dimensions(), si.GetBufferSize().Dimensions());
    VERIFY_ARE_EQUAL(si.GetViewport(), si.GetBufferSize());

    if (clearBufferMethod == ClearLikeCls)
    {
        // Execute the cls, EXACTLY LIKE CMD.

        CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        _apiRoutines.GetConsoleScreenBufferInfoExImpl(si, csbiex);

        SMALL_RECT src{ 0 };
        src.Top = 0;
        src.Left = 0;
        src.Right = csbiex.dwSize.X;
        src.Bottom = csbiex.dwSize.Y;

        COORD tgt{ 0, -csbiex.dwSize.Y };
        VERIFY_SUCCEEDED(_apiRoutines.ScrollConsoleScreenBufferWImpl(si,
                                                                     src,
                                                                     tgt,
                                                                     std::nullopt, // no clip provided,
                                                                     L' ',
                                                                     csbiex.wAttributes,
                                                                     true));
    }
    else if (clearBufferMethod == ClearLikeClearHost)
    {
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        _apiRoutines.GetConsoleScreenBufferInfoExImpl(si, csbiex);

        const auto totalCellsInBuffer = csbiex.dwSize.X * csbiex.dwSize.Y;
        size_t cellsWritten = 0;
        VERIFY_SUCCEEDED(_apiRoutines.FillConsoleOutputCharacterWImpl(si,
                                                                      L' ',
                                                                      totalCellsInBuffer,
                                                                      { 0, 0 },
                                                                      cellsWritten,
                                                                      true));
        VERIFY_SUCCEEDED(_apiRoutines.FillConsoleOutputAttributeImpl(si,
                                                                     csbiex.wAttributes,
                                                                     totalCellsInBuffer,
                                                                     { 0, 0 },
                                                                     cellsWritten));
    }
    else if (clearBufferMethod == ClearWithVT)
    {
        sm.ProcessString(L"\x1b[2J");
        VERIFY_ARE_EQUAL(si.GetViewport().Dimensions(), si.GetBufferSize().Dimensions());
        VERIFY_ARE_EQUAL(si.GetViewport(), si.GetBufferSize());

        sm.ProcessString(L"\x1b[3J");
    }

    VERIFY_ARE_EQUAL(si.GetViewport().Dimensions(), si.GetBufferSize().Dimensions());
    VERIFY_ARE_EQUAL(si.GetViewport(), si.GetBufferSize());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the host buffer state (after) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), true);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());
    Log::Comment(L"========== Checking the terminal buffer state (after) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive(), true);
}

void ConptyRoundtripTests::TestResizeWithCookedRead()
{
    // see https://github.com/microsoft/terminal/issues/1856
    Log::Comment(L"This test checks a crash in conpty where resizing the "
                 L"window with any data in a cooked read (like the input line "
                 L"in cmd.exe) would cause the conpty to crash.");

    // Resizing with a COOKED_READ used to cause a crash in
    // `Selection::s_GetInputLineBoundaries` north of
    // `Selection::GetValidAreaBoundaries`.
    //
    // If this test completes successfully, then we know that we didn't crash.

    // The specific cases that repro the original crash are:
    // * (0, -10)
    // * (0, -1)
    // * (0, 0)
    // the rest of the cases are added here for completeness.

    // Don't let the cooked read pollute other tests
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-10, -1, 0, 1, 10}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
    END_TEST_METHOD_PROPERTIES()

    INIT_TEST_PROPERTY(int, dx, L"The change in width of the buffer");
    INIT_TEST_PROPERTY(int, dy, L"The change in height of the buffer");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    // Setup the cooked read data
    m_state->PrepareReadHandle();
    // TODO GH#5618: This string will get mangled, but we don't really care
    // about the buffer contents in this test, so it doesn't really matter.
    const std::string_view cookedReadContents{ "This is some cooked read data" };
    m_state->PrepareCookedReadData(cookedReadContents);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // After we resize, make sure to get the new textBuffers
    std::tie(hostTb, termTb) = _performResize({ TerminalViewWidth + dx,
                                                TerminalViewHeight + dy });

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // By simply reaching the end of this test, we know that we didn't crash. Hooray!
}

void ConptyRoundtripTests::ResizeInitializeBufferWithDefaultAttrs()
{
    // See https://github.com/microsoft/terminal/issues/3848
    Log::Comment(L"This test checks that the attributes in the text buffer are "
                 L"initialized to a sensible value during a resize. The entire "
                 L"buffer shouldn't be filled with _whatever the current "
                 L"attributes are_, it should be filled with the default "
                 L"attributes (however the application defines that). Then, "
                 L"after the resize, we should still be able to print to the "
                 L"buffer with the old \"current attributes\"");

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dx", L"{-1, 0, 1}")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-1, 0, 1}")
        TEST_METHOD_PROPERTY(L"Data:leaveTrailingChar", L"{false, true}")
    END_TEST_METHOD_PROPERTIES()

    INIT_TEST_PROPERTY(int, dx, L"The change in width of the buffer");
    INIT_TEST_PROPERTY(int, dy, L"The change in height of the buffer");
    INIT_TEST_PROPERTY(bool, leaveTrailingChar, L"If true, we'll print one additional '#' on row 3");

    // Do nothing if the resize would just be a no-op.
    if (dx == 0 && dy == 0)
    {
        return;
    }

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    auto defaultAttrs = si.GetAttributes();
    auto conhostGreenAttrs = TextAttribute();

    // Conhost and Terminal store attributes in different bits.
    // conhostGreenAttrs.SetIndexedAttributes(std::nullopt,
    //                                        { static_cast<BYTE>(FOREGROUND_GREEN) });
    conhostGreenAttrs.SetIndexedBackground(FOREGROUND_GREEN);
    auto terminalGreenAttrs = TextAttribute();
    // terminalGreenAttrs.SetIndexedAttributes(std::nullopt,
    //                                         { static_cast<BYTE>(XTERM_GREEN_ATTR) });
    terminalGreenAttrs.SetIndexedBackground(XTERM_GREEN_ATTR);

    const size_t width = static_cast<size_t>(TerminalViewWidth);

    // Use an initial ^[[m to start printing with default-on-default
    sm.ProcessString(L"\x1b[m");

    // Print three lines with "# #", where the first "# " are in
    // default-on-green.
    for (int i = 0; i < 3; i++)
    {
        sm.ProcessString(L"\x1b[42m");
        sm.ProcessString(L"# ");
        sm.ProcessString(L"\x1b[m");
        sm.ProcessString(L"#");
        sm.ProcessString(L"\r\n");
    }

    // Now, leave the active attributes as default-on-green. When we resize the
    // buffers, we don't want them initialized with default-on-green, we want
    // them to use whatever the set default attributes are.
    sm.ProcessString(L"\x1b[42m");

    // If leaveTrailingChar is true, we'll leave one default-on-green '#' on row
    // 3. This will force conpty to change the Terminal's colors to
    // default-on-green, so we can check that not only conhost initialize the
    // buffer colors correctly, but so does the Terminal.
    if (leaveTrailingChar)
    {
        sm.ProcessString(L"#");
    }

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport, const bool isTerminal, const bool afterResize) {
        const auto width = viewport.width<short>();

        // Conhost and Terminal store attributes in different bits.
        const auto greenAttrs = isTerminal ? terminalGreenAttrs : conhostGreenAttrs;

        for (short row = 0; row < tb.GetSize().Height(); row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d...", row));

            VERIFY_IS_FALSE(tb.GetRowByOffset(row).WasWrapForced());

            const bool hasChar = row < 3;
            const auto actualDefaultAttrs = isTerminal ? TextAttribute() : defaultAttrs;

            if (hasChar)
            {
                auto iter = TestUtils::VerifyLineContains(tb, { 0, row }, L'#', greenAttrs, 1u);
                TestUtils::VerifyLineContains(iter, L' ', greenAttrs, 1u);
                TestUtils::VerifyLineContains(iter, L'#', TextAttribute(), 1u);
                // After the resize, the default attrs of the last char will
                // extend to fill the rest of the row. This is GH#32. If that
                // bug ever gets fixed, this test will break, but that's
                // ABSOLUTELY OKAY.
                TestUtils::VerifyLineContains(iter, L' ', (afterResize ? TextAttribute() : actualDefaultAttrs), static_cast<size_t>(width - 3));
            }
            else if (leaveTrailingChar && row == 3)
            {
                auto iter = TestUtils::VerifyLineContains(tb, { 0, row }, L'#', greenAttrs, 1u);
                TestUtils::VerifyLineContains(iter, L' ', (afterResize ? greenAttrs : actualDefaultAttrs), static_cast<size_t>(width - 1));
            }
            else
            {
                TestUtils::VerifyLineContains(tb, { 0, row }, L' ', actualDefaultAttrs, viewport.width<size_t>());
            }
        }
    };

    Log::Comment(L"========== Checking the host buffer state (before) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), false, false);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (before) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive(), true, false);

    // After we resize, make sure to get the new textBuffers
    std::tie(hostTb, termTb) = _performResize({ TerminalViewWidth + dx,
                                                TerminalViewHeight + dy });

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the host buffer state (after) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), false, true);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state (after) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive(), true, true);
}

void ConptyRoundtripTests::NewLinesAtBottomWithBackground()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:paintEachNewline", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:spacesToPrint", L"{1, 7, 8, 9, 32}")
    END_TEST_METHOD_PROPERTIES();

    INIT_TEST_PROPERTY(bool, paintEachNewline, L"If true, call PaintFrame after each pair of lines.");
    INIT_TEST_PROPERTY(int, spacesToPrint, L"Controls the number of spaces printed after the first '#'");

    // See https://github.com/microsoft/terminal/issues/5502
    Log::Comment(L"Attempts to emit text to a new bottom line with spaces with "
                 L"a colored background. When that happens, we should make "
                 L"sure to still print the spaces, because the information "
                 L"about their background color is important.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    auto defaultAttrs = si.GetAttributes();
    auto conhostBlueAttrs = defaultAttrs;

    // Conhost and Terminal store attributes in different bits.
    conhostBlueAttrs.SetIndexedForeground(FOREGROUND_GREEN);
    conhostBlueAttrs.SetIndexedBackground(FOREGROUND_BLUE);
    auto terminalBlueAttrs = TextAttribute();
    terminalBlueAttrs.SetIndexedForeground(XTERM_GREEN_ATTR);
    terminalBlueAttrs.SetIndexedBackground(XTERM_BLUE_ATTR);

    const size_t width = static_cast<size_t>(TerminalViewWidth);

    // We're going to print 4 more rows than the entire height of the viewport,
    // causing the buffer to circle 4 times. This is 2 extra iterations of the
    // two lines we're printing per iteration.
    const auto circledRows = 4;
    for (auto i = 0; i < (TerminalViewHeight + circledRows) / 2; i++)
    {
        // We're printing pairs of lines: ('_' is a space character)
        //
        // Line 1 chars: __________________ (break)
        // Line 1 attrs: DDDDDDDDDDDDDDDDDD (all default attrs)
        // Line 2 chars: ____#_________#___ (break)
        // Line 2 attrs: BBBBBBBBBBBBBBDDDD (First spacesToPrint+5 are blue BG, then default attrs)
        //                    [<----->]
        //                      This number of spaces controled by spacesToPrint
        if (i > 0)
        {
            sm.ProcessString(L"\r\n");
        }

        // In WSL:
        // echo -e "\e[m\r\n\e[44;32m    #         \e[m#"

        sm.ProcessString(L"\x1b[m");
        sm.ProcessString(L"\r");
        sm.ProcessString(L"\n");
        sm.ProcessString(L"\x1b[44;32m");
        sm.ProcessString(L"    #");
        sm.ProcessString(std::wstring(spacesToPrint, L' '));
        sm.ProcessString(L"\x1b[m");
        sm.ProcessString(L"#");

        if (paintEachNewline)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }
    }

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport) {
        const auto width = viewport.width<short>();
        const auto isTerminal = viewport.top() != 0;

        // Conhost and Terminal store attributes in different bits.
        const auto blueAttrs = isTerminal ? terminalBlueAttrs : conhostBlueAttrs;

        for (short row = 0; row < viewport.bottom<short>() - 2; row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));
            VERIFY_IS_FALSE(tb.GetRowByOffset(row).WasWrapForced());

            const auto isBlank = (row % 2) == 0;
            const auto rowCircled = row > (viewport.bottom<short>() - 1 - circledRows);
            // When the buffer circles, new lines will be initialized using the
            // current text attributes. Those will be the default-on-default
            // attributes. All of the Terminal's buffer will use
            // default-on-default.
            const auto actualDefaultAttrs = rowCircled || isTerminal ? TextAttribute() : defaultAttrs;
            Log::Comment(NoThrowString().Format(L"isBlank=%d, rowCircled=%d", isBlank, rowCircled));

            if (isBlank)
            {
                TestUtils::VerifyLineContains(tb, { 0, row }, L' ', actualDefaultAttrs, viewport.width<size_t>());
            }
            else
            {
                auto iter = TestUtils::VerifyLineContains(tb, { 0, row }, L' ', blueAttrs, 4u);
                TestUtils::VerifyLineContains(iter, L'#', blueAttrs, 1u);
                TestUtils::VerifyLineContains(iter, L' ', blueAttrs, static_cast<size_t>(spacesToPrint));
                TestUtils::VerifyLineContains(iter, L'#', TextAttribute(), 1u);
                TestUtils::VerifyLineContains(iter, L' ', actualDefaultAttrs, static_cast<size_t>(width - 15));
            }
        }
    };

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());
}

void doWriteCharsLegacy(SCREEN_INFORMATION& screenInfo, const std::wstring_view string, DWORD flags = 0)
{
    size_t dwNumBytes = string.size() * sizeof(wchar_t);
    VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(screenInfo,
                                             string.data(),
                                             string.data(),
                                             string.data(),
                                             &dwNumBytes,
                                             nullptr,
                                             screenInfo.GetTextBuffer().GetCursor().GetPosition().X,
                                             flags,
                                             nullptr));
}

void ConptyRoundtripTests::WrapNewLineAtBottom()
{
    // The actual bug case is
    // * paintEachNewline=2 (PaintEveryLine)
    // * writingMethod=1 (PrintWithWriteCharsLegacy)
    // * circledRows=4
    //
    // Though, mysteriously, the bug that this test caught _WASN'T_ the fix for
    // #5691

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:paintEachNewline", L"{0, 1, 2}")
        TEST_METHOD_PROPERTY(L"Data:writingMethod", L"{0, 1}")
        TEST_METHOD_PROPERTY(L"Data:circledRows", L"{2, 4, 10}")
    END_TEST_METHOD_PROPERTIES();

    // By modifying how often we call PaintFrame, we can test if different
    // timings for the frame affect the results. In this test we'll be printing
    // a bunch of paired lines. These values control when the PaintFrame calls
    // will occur:
    constexpr int DontPaint = 0; // Only paint at the end of all the output
    constexpr int PaintAfterBothLines = 1; // Paint after each pair of lines is output
    constexpr int PaintEveryLine = 2; // Paint after each and every line is output.

    constexpr int PrintWithPrintString = 0;
    constexpr int PrintWithWriteCharsLegacy = 1;

    INIT_TEST_PROPERTY(int, writingMethod, L"Controls using either ProcessString or WriteCharsLegacy to write to the buffer");
    INIT_TEST_PROPERTY(int, circledRows, L"Controls the number of lines we output.");
    INIT_TEST_PROPERTY(int, paintEachNewline, L"Controls whether we should call PaintFrame every line of text or not.");

    // GH#5839 -
    // This test does expose a real bug when using WriteCharsLegacy to emit
    // wrapped lines in conpty without WC_DELAY_EOL_WRAP. However, this fix has
    // not yet been made, so for now, we need to just skip the cases that cause
    // this.
    if (writingMethod == PrintWithWriteCharsLegacy && paintEachNewline == PaintEveryLine)
    {
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    // I've tested this with 0x0, 0x4, 0x80, 0x84, and 0-8, and none of these
    // flags seem to make a difference. So we're just assuming 0 here, so we
    // don't test a bunch of redundant cases.
    const auto writeCharsLegacyMode = 0;

    // This test was originally written for
    //   https://github.com/microsoft/terminal/issues/5691
    //
    // It does not _actually_ test #5691 however, it merely checks another issue
    // found during debugging.

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    const size_t width = static_cast<size_t>(TerminalViewWidth);

    const auto charsInFirstLine = width;
    const auto numCharsFirstColor = 30;
    const auto numCharsSecondColor = charsInFirstLine - numCharsFirstColor;
    const auto charsInSecondLine = width / 2;

    const auto defaultAttrs = TextAttribute();
    const auto conhostDefaultAttrs = si.GetAttributes();

    // Helper to abstract calls into either StateMachine::ProcessString or
    // WriteCharsLegacy
    auto print = [&](const std::wstring_view str) {
        if (writingMethod == PrintWithPrintString)
        {
            sm.ProcessString(str);
        }
        else if (writingMethod == PrintWithWriteCharsLegacy)
        {
            doWriteCharsLegacy(si, str, writeCharsLegacyMode);
        }
    };

    // Each of the lines of text in the buffer will look like the following:
    //
    // Line 1 chars: ~~~~~~~~~~~~~~~~~~ <wrap>
    // Line 1 attrs: YYYYYYYDDDDDDDDDDD (First 30 are yellow FG, then default attrs)
    // Line 2 chars: ~~~~~~~~~~________ <break> (there are width/2 '~'s here)
    // Line 2 attrs: DDDDDDDDDDDDDDDDDD (all are default attrs)
    //

    for (auto i = 0; i < (TerminalViewHeight + circledRows) / 2; i++)
    {
        Log::Comment(NoThrowString().Format(L"writing pair of lines %d", i));

        if (i > 0)
        {
            sm.ProcessString(L"\r\n");
        }

        sm.ProcessString(L"\x1b[33m");

        print(std::wstring(numCharsFirstColor, L'~'));

        sm.ProcessString(L"\x1b[m");

        print(std::wstring(numCharsSecondColor, L'~'));

        // If we're painting every line, then paint now, while conpty is in a
        // deferred EOL state. Otherwise, we'll wait to paint till after more output.
        if (paintEachNewline == PaintEveryLine)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }

        // Print the rest of the string of text, which should continue from the
        // wrapped line before it.
        print(std::wstring(charsInSecondLine, L'~'));

        if (paintEachNewline == PaintEveryLine ||
            paintEachNewline == PaintAfterBothLines)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }
    }

    // At this point, the entire buffer looks like:
    //
    // row[0]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[1]: |~~~~~~             | <break>
    // row[2]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[3]: |~~~~~~             | <break>
    // row[4]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[5]: |~~~~~~             | <break>

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport) {
        const auto width = viewport.width<short>();
        const auto isTerminal = viewport.top() != 0;

        for (short row = 0; row < viewport.bottom<short>(); row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));

            // The first line wrapped, the second didn't, so on and so forth
            const auto isWrapped = (row % 2) == 0;
            const auto rowCircled = row >= (viewport.bottom<short>() - circledRows);

            const auto actualNonSpacesAttrs = defaultAttrs;
            const auto actualSpacesAttrs = rowCircled || isTerminal ? defaultAttrs : conhostDefaultAttrs;

            VERIFY_ARE_EQUAL(isWrapped, tb.GetRowByOffset(row).WasWrapForced());
            if (isWrapped)
            {
                TestUtils::VerifyExpectedString(tb, std::wstring(charsInFirstLine, L'~'), til::point{ 0, row });
            }
            else
            {
                auto iter = TestUtils::VerifyExpectedString(tb, std::wstring(charsInSecondLine, L'~'), til::point{ 0, row });
                TestUtils::VerifyExpectedString(std::wstring(width - charsInSecondLine, L' '), iter);
            }
        }
    };

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");
    VERIFY_ARE_EQUAL(circledRows, term->_mutableViewport.Top());
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::WrapNewLineAtBottomLikeMSYS()
{
    // See https://github.com/microsoft/terminal/issues/5691
    Log::Comment(L"This test attempts to print text like the MSYS `less` pager "
                 L"does. When it prints a wrapped line, we should make sure to "
                 L"not break the line wrapping.");

    // The importantly valuable variable here ended up being
    // writingMethod=PrintWithWriteCharsLegacy. That was the one thing that
    // actually repro'd this bug. The other variables were introduced as part of
    // debugging, and are left for completeness.
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:circledRows", L"{2, 4, 10}")
        TEST_METHOD_PROPERTY(L"Data:paintEachNewline", L"{0, 1, 2}")
        TEST_METHOD_PROPERTY(L"Data:writingMethod", L"{0, 1}")
    END_TEST_METHOD_PROPERTIES();

    // By modifying how often we call PaintFrame, we can test if different
    // timings for the frame affect the results. In this test we'll be printing
    // a bunch of paired lines. These values control when the PaintFrame calls
    // will occur:
    constexpr int DontPaint = 0; // Only paint at the end of all the output
    constexpr int PaintAfterBothLines = 1; // Paint after each pair of lines is output
    constexpr int PaintEveryLine = 2; // Paint after each and every line is output.

    constexpr int PrintWithPrintString = 0;
    constexpr int PrintWithWriteCharsLegacy = 1;

    INIT_TEST_PROPERTY(int, writingMethod, L"Controls using either ProcessString or WriteCharsLegacy to write to the buffer");
    INIT_TEST_PROPERTY(int, circledRows, L"Controls the number of lines we output.");
    INIT_TEST_PROPERTY(int, paintEachNewline, L"Controls whether we should call PaintFrame every line of text or not.");

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    const size_t width = static_cast<size_t>(TerminalViewWidth);

    const auto charsInFirstLine = width;
    const auto numCharsFirstColor = 30;
    const auto numCharsSecondColor = charsInFirstLine - numCharsFirstColor;
    const auto charsInSecondLine = width / 2;

    const auto defaultAttrs = TextAttribute();
    const auto conhostDefaultAttrs = si.GetAttributes();

    // Helper to abstract calls into either StateMachine::ProcessString or
    // WriteCharsLegacy
    auto print = [&](const std::wstring_view str) {
        if (writingMethod == PrintWithPrintString)
        {
            sm.ProcessString(str);
        }
        else if (writingMethod == PrintWithWriteCharsLegacy)
        {
            doWriteCharsLegacy(si, str, WC_LIMIT_BACKSPACE);
        }
    };

    // Each of the lines of text in the buffer will look like the following:
    //
    // Line 1 chars: ~~~~~~~~~~~~~~~~~~ <wrap>
    // Line 1 attrs: YYYYYYYDDDDDDDDDDD (First 30 are yellow FG, then default attrs)
    // Line 2 chars: ~~~~~~~~~~________ <break> (there are width/2 '~'s here)
    // Line 2 attrs: DDDDDDDDDDDDDDDDDD (all are default attrs)
    //
    // The last line of the buffer will be used as a "prompt" line, with a
    // single ':' in it. This is similar to the way `less` typically displays
    // it's prompt at the bottom of the buffer.

    // First, print a whole viewport full of text.
    for (auto i = 0; i < (TerminalViewHeight) / 2; i++)
    {
        Log::Comment(NoThrowString().Format(L"writing pair of lines %d", i));

        sm.ProcessString(L"\x1b[33m");
        print(std::wstring(numCharsFirstColor, L'~'));
        sm.ProcessString(L"\x1b[m");

        if (paintEachNewline == PaintEveryLine)
        {
            print(std::wstring(numCharsSecondColor, L'~'));
            VERIFY_SUCCEEDED(renderer.PaintFrame());
            print(std::wstring(charsInSecondLine, L'~'));
        }
        else
        {
            // If we're not painting each and every line, then just print all of
            // the wrapped text in one go. These '~'s will wrap from the first
            // line onto the second line.
            print(std::wstring(numCharsSecondColor + charsInSecondLine, L'~'));
        }

        sm.ProcessString(L"\r\n");

        if (paintEachNewline == PaintEveryLine ||
            paintEachNewline == PaintAfterBothLines)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }
    }

    // Then print the trailing ':' on the last line.
    print(L":");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // At this point, the entire buffer looks like:
    //
    // row[25]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[26]: |~~~~~~             | <break>
    // row[27]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[28]: |~~~~~~             | <break>
    // row[29]: |~~~~~~~~~~~~~~~~~~~| <wrap>
    // row[30]: |~~~~~~             | <break>
    // row[31]: |:                  | <break>

    // Now, we'll print the lines that wrapped, after circling the buffer.
    for (auto i = 0; i < (circledRows) / 2; i++)
    {
        Log::Comment(NoThrowString().Format(L"writing pair of lines %d", i + (TerminalViewHeight / 2)));

        print(L"\r");
        sm.ProcessString(L"\x1b[33m");
        print(std::wstring(numCharsFirstColor, L'~'));
        sm.ProcessString(L"\x1b[m");

        if (paintEachNewline == PaintEveryLine)
        {
            // If we're painting every line, then we'll print the first line and
            // redraw the "prompt" line (with the ':'), paint the buffer, then
            // print the second line and reprint the prompt, as if the user was
            // slowly hitting the down arrow one line per frame.
            print(std::wstring(numCharsSecondColor, L'~'));
            print(L":");
            VERIFY_SUCCEEDED(renderer.PaintFrame());
            print(L"\r");
            print(std::wstring(charsInSecondLine, L'~'));
        }
        else
        {
            // Otherwise, we'll print the wrapped line all in one frame.
            print(std::wstring(numCharsSecondColor + charsInSecondLine, L'~'));
        }

        // Print the prompt at the bottom of the buffer
        print(L"\r\n");
        print(L":");

        if (paintEachNewline == PaintEveryLine ||
            paintEachNewline == PaintAfterBothLines)
        {
            VERIFY_SUCCEEDED(renderer.PaintFrame());
        }
    }

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport) {
        const auto width = viewport.width<short>();
        const auto isTerminal = viewport.top() != 0;
        auto lastRow = viewport.bottom<short>() - 1;
        for (short row = 0; row < lastRow; row++)
        {
            Log::Comment(NoThrowString().Format(L"Checking row %d", row));

            // The first line wrapped, the second didn't, so on and so forth.
            // However, because conpty's buffer is only as tall as the viewport,
            // we're going to lose lines off the top of the buffer. Most
            // importantly, because we'll have the "prompt" line in the conpty
            // buffer, then the top line of the conpty will _not_ be wrapped,
            // when the 0th line of the terminal buffer _is_.
            const auto isWrapped = (row % 2) == (isTerminal ? 0 : 1);
            const auto rowCircled = row >= (viewport.bottom<short>() - circledRows);

            const auto actualNonSpacesAttrs = defaultAttrs;
            const auto actualSpacesAttrs = rowCircled || isTerminal ? defaultAttrs : conhostDefaultAttrs;

            VERIFY_ARE_EQUAL(isWrapped, tb.GetRowByOffset(row).WasWrapForced());
            if (isWrapped)
            {
                TestUtils::VerifyExpectedString(tb, std::wstring(charsInFirstLine, L'~'), til::point{ 0, row });
            }
            else
            {
                auto iter = TestUtils::VerifyExpectedString(tb, std::wstring(charsInSecondLine, L'~'), til::point{ 0, row });
                TestUtils::VerifyExpectedString(std::wstring(width - charsInSecondLine, L' '), iter);
            }
        }
        VERIFY_IS_FALSE(tb.GetRowByOffset(lastRow).WasWrapForced());
        auto iter = TestUtils::VerifyExpectedString(tb, std::wstring(1, L':'), til::point{ 0, lastRow });
        TestUtils::VerifyExpectedString(std::wstring(width - 1, L' '), iter);
    };

    Log::Comment(L"========== Checking the host buffer state ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive());

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    Log::Comment(L"========== Checking the terminal buffer state ==========");
    VERIFY_ARE_EQUAL(circledRows + 1, term->_mutableViewport.Top());
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive());
}

void ConptyRoundtripTests::DeleteWrappedWord()
{
    // See https://github.com/microsoft/terminal/issues/5839
    Log::Comment(L"This test ensures that when we print a empty row beneath a "
                 L"wrapped row, that we _actually_ clear it.");
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& sm = si.GetStateMachine();
    auto* hostTb = &si.GetTextBuffer();
    auto* termTb = term->_buffer.get();

    _flushFirstFrame();

    _checkConptyOutput = false;
    _logConpty = true;

    // Print two lines of text:
    // |AAAAAAAAAAAAA BBBBBB| <wrap>
    // |BBBBBBBB_           | <break>
    // (cursor on the '_')

    sm.ProcessString(L"\x1b[?25l");
    sm.ProcessString(std::wstring(50, L'A'));
    sm.ProcessString(L" ");
    sm.ProcessString(std::wstring(50, L'B'));
    sm.ProcessString(L"\x1b[?25h");

    auto verifyBuffer = [&](const TextBuffer& tb, const til::rectangle viewport, const bool after) {
        const auto width = viewport.width<short>();

        auto iter1 = tb.GetCellDataAt({ 0, 0 });
        TestUtils::VerifySpanOfText(L"A", iter1, 0, 50);
        TestUtils::VerifySpanOfText(L" ", iter1, 0, 1);
        if (after)
        {
            TestUtils::VerifySpanOfText(L" ", iter1, 0, 50);

            auto iter2 = tb.GetCellDataAt({ 0, 1 });
            TestUtils::VerifySpanOfText(L" ", iter2, 0, width);
        }
        else
        {
            TestUtils::VerifySpanOfText(L"B", iter1, 0, 50);

            auto iter2 = tb.GetCellDataAt({ 0, 1 });
            TestUtils::VerifySpanOfText(L"B", iter2, 0, 50 - (width - 51));
            TestUtils::VerifySpanOfText(L" ", iter2, 0, width);
        }
    };

    Log::Comment(L"========== Checking the host buffer state (before) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), false);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());
    Log::Comment(L"========== Checking the terminal buffer state (before) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive(), false);

    // Now, go back and erase all the 'B's, as if the user executed a
    // backward-kill-word in PowerShell. Afterwards, the buffer will look like:
    //
    // |AAAAAAAAAAAAA_      |
    // |                    |
    //
    // We're doing this the same way PsReadline redraws the prompt - by just
    // reprinting all of it.

    sm.ProcessString(L"\x1b[?25l");
    sm.ProcessString(L"\x1b[H");
    sm.ProcessString(std::wstring(50, L'A'));
    sm.ProcessString(L" ");

    sm.ProcessString(std::wstring(TerminalViewWidth - 51, L' '));

    sm.ProcessString(L"\x1b[2;1H");
    sm.ProcessString(std::wstring(50 - (TerminalViewWidth - 51), L' '));
    sm.ProcessString(L"\x1b[1;50H");
    sm.ProcessString(L"\x1b[?25h");

    Log::Comment(L"========== Checking the host buffer state (after) ==========");
    verifyBuffer(*hostTb, si.GetViewport().ToInclusive(), true);

    Log::Comment(L"Painting the frame");
    VERIFY_SUCCEEDED(renderer.PaintFrame());
    Log::Comment(L"========== Checking the terminal buffer state (after) ==========");
    verifyBuffer(*termTb, term->_mutableViewport.ToInclusive(), true);
}

// This test checks that upon conpty rendering again, terminal still maintains
// the same hyperlink IDs
void ConptyRoundtripTests::HyperlinkIdConsistency()
{
    Log::Comment(NoThrowString().Format(
        L"Write a link - the text will simply be 'Link' and the uri will be 'http://example.com'"));

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;

    _flushFirstFrame();

    hostSm.ProcessString(L"\x1b]8;;http://example.com\x1b\\Link\x1b]8;;\x1b\\");

    // For self-generated IDs, conpty will send a custom ID of the form
    // {sessionID}-{self-generated ID}
    // self-generated IDs begin at 1 and increment from there
    const std::string fmt{ "\x1b]8;id={}-1;http://example.com\x1b\\" };
    auto s = fmt::format(fmt, GetCurrentProcessId());
    expectedOutput.push_back(s);
    expectedOutput.push_back("Link");
    expectedOutput.push_back("\x1b]8;;\x1b\\");

    // Force a frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Move the cursor down
    hostSm.ProcessString(L"\x1b[2;1H");
    expectedOutput.push_back("\r\n");

    // Force a frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Move the cursor to somewhere in the link text
    hostSm.ProcessString(L"\x1b[1;2H");
    expectedOutput.push_back("\x1b[1;2H");
    expectedOutput.push_back("\x1b[?25h");

    // Force a frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Move the cursor off the link
    hostSm.ProcessString(L"\x1b[2;1H");
    expectedOutput.push_back("\r\n");

    // Force a frame
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    auto verifyData = [](TextBuffer& tb) {
        // Check that all the linked cells still have the same ID
        auto& attrRow = tb.GetRowByOffset(0).GetAttrRow();
        auto id = attrRow.GetAttrByColumn(0).GetHyperlinkId();
        for (uint16_t i = 1; i < 4; ++i)
        {
            VERIFY_ARE_EQUAL(id, attrRow.GetAttrByColumn(i).GetHyperlinkId());
        }
    };

    verifyData(hostTb);
    verifyData(termTb);
}
