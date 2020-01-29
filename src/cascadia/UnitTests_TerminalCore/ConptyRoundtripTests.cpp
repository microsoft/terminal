// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This test class creates an in-proc conpty host as well as a Terminal, to
// validate that strings written to the conpty create the same resopnse on the
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
    class TerminalBufferTests;
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

        g.pRender->AddRenderEngine(_pVtRenderEngine.get());
        gci.GetActiveOutputBuffer().SetTerminalConnection(_pVtRenderEngine.get());

        _pConApi = std::make_unique<ConhostInternalGetSet>(gci);

        // Manually set the console into conpty mode. We're not actually going
        // to set up the pipes for conpty, but we want the console to behave
        // like it would in conpty mode.
        g.EnableConptyModeForTests();

        expectedOutput.clear();

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
