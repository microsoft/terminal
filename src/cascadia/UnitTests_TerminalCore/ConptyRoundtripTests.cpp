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
    static const SHORT TerminalViewWidth = 80;
    static const SHORT TerminalViewHeight = 32;

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

    TEST_METHOD(TestWrappingALongString);
    TEST_METHOD(TestAdvancedWrapping);
    TEST_METHOD(TestExactWrappingWithoutSpaces);
    TEST_METHOD(TestExactWrappingWithSpaces);

    TEST_METHOD(MoveCursorAtEOL);

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
    expectedOutput.push_back("\r\n");
    // Here, we're going to emit 3 spaces. The region that got invalidated was a
    // rectangle from 0,0 to 3,3, so the vt renderer will try to render the
    // region in between BBB and CCC as well, because it got included in the
    // rectangle Or() operation.
    // This behavior should not be seen as binding - if a future optimization
    // breaks this test, it wouldn't be the worst.
    expectedOutput.push_back("   ");
    expectedOutput.push_back("\r\n");
    expectedOutput.push_back("CCC");

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
    // Clear the rest of row 1
    expectedOutput.push_back("\x1b[K");
    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("          1234567890");
    // and clear everything after the text, because the buffer is empty.
    expectedOutput.push_back("\x1b[K");
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
    // that's exactly the width of the buffer that manually linebreaked at the
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

    auto verifyBuffer = [&](const TextBuffer& tb, const bool isTerminal) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(10, cursor.GetPosition().X);

        // TODO: GH#780 - In the Terminal, neither line should be wrapped.
        // Unfortunately, until WriteCharsLegacy2ElectricBoogaloo is complete,
        // the Terminal will still treat the first line as wrapped. When #780 is
        // implemented, these tests will fail, and should again expect the first
        // line to not be wrapped.

        // Verify that we marked the 0th row as _not wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_ARE_EQUAL(isTerminal, row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, LR"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"1234567890", { 0, 1 });
    };

    verifyBuffer(hostTb, false);

    // First write the first 80 characters from the string
    expectedOutput.push_back(R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)");

    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("1234567890");
    // and clear everything after the text, because the buffer is empty.
    expectedOutput.push_back("\x1b[K");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb, true);
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

    auto verifyBuffer = [&](const TextBuffer& tb, const bool isTerminal) {
        auto& cursor = tb.GetCursor();
        // Verify the cursor wrapped to the second line
        VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);
        VERIFY_ARE_EQUAL(20, cursor.GetPosition().X);

        // TODO: GH#780 - In the Terminal, neither line should be wrapped.
        // Unfortunately, until WriteCharsLegacy2ElectricBoogaloo is complete,
        // the Terminal will still treat the first line as wrapped. When #780 is
        // implemented, these tests will fail, and should again expect the first
        // line to not be wrapped.

        // Verify that we marked the 0th row as _not wrapped_
        const auto& row0 = tb.GetRowByOffset(0);
        VERIFY_ARE_EQUAL(isTerminal, row0.GetCharRow().WasWrapForced());

        const auto& row1 = tb.GetRowByOffset(1);
        VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

        TestUtils::VerifyExpectedString(tb, LR"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)", { 0, 0 });
        TestUtils::VerifyExpectedString(tb, L"          1234567890", { 0, 1 });
    };

    verifyBuffer(hostTb, false);

    // First write the first 80 characters from the string
    expectedOutput.push_back(R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnop)");

    // This is the hard line break
    expectedOutput.push_back("\r\n");
    // Now write row 2 of the buffer
    expectedOutput.push_back("          1234567890");
    // and clear everything after the text, because the buffer is empty.
    expectedOutput.push_back("\x1b[K");
    VERIFY_SUCCEEDED(renderer.PaintFrame());

    verifyBuffer(termTb, true);
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

void ConptyRoundtripTests::PassthroughClearScrollback()
{
    Log::Comment(NoThrowString().Format(
        L"Write more lines of outout. We should use \r\n to move the cursor"));
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
            // seperated for whatever reason.
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

    // Verify the top of the Terminal veiwoprt contains the contents of the old viewport
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
