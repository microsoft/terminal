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
    // and clear everything after the text, because the buffer is empty.
    expectedOutput.push_back("\x1b[K");
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
    // and clear everything after the text, because the buffer is empty.
    expectedOutput.push_back("\x1b[K");
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
            // separated for whatever reason.

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
