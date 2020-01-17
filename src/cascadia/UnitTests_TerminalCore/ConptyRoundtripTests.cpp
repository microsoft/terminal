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
        // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // This doesn't actually set the host into VtIo mode. That gci function
        // requires the VtIo to actually be initialized, which it wont be in
        // this case. We should instead add some sort of ut-only method to trick
        // gci into thinking it's actually in conpty mode.

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
    TEST_METHOD(TestResizeHeight);

private:
    bool _writeCallback(const char* const pch, size_t const cch);
    void _flushFirstFrame();
    void _resizeConpty(const unsigned short sx, const unsigned short sy);
    std::deque<std::string> expectedOutput;
    std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
    std::unique_ptr<CommonState> m_state;
    std::unique_ptr<Microsoft::Console::VirtualTerminal::ConGetSet> _pConApi;

    bool _checkConptyOutput{ true };

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
                               NoThrowString().Format(L"writing=\"%hs\", expecting %u strings", actualString.c_str(), expectedOutput.size()));

        std::string first = expectedOutput.front();
        expectedOutput.pop_front();

        Log::Comment(NoThrowString().Format(L"Expected =\t\"%hs\"", first.c_str()));
        Log::Comment(NoThrowString().Format(L"Actual =\t\"%hs\"", actualString.c_str()));

        VERIFY_ARE_EQUAL(first.length(), cch);
        VERIFY_ARE_EQUAL(first, actualString);
    }
    else
    {
        Log::Comment(NoThrowString().Format(
            L"Writing \"%hs\" to Terminal", actualString.c_str()));
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

// Function Description:
// - Helper function to validate that the next characters pointed to by `iter`
//   are the provided string. Will increment iter as it walks the provided
//   string of characters. It will leave `iter` on the first character after the
//   expectedString.
// Arguments:
// - expectedString: The characters we're expecting
// - iter: a iterator pointing to the cell we'd like to start validating at.
// Return Value:
// - <none>
void _verifyExpectedString(std::wstring_view expectedString,
                           TextBufferCellIterator& iter)
{
    for (const auto wch : expectedString)
    {
        wchar_t buffer[]{ wch, L'\0' };
        std::wstring_view view{ buffer, 1 };
        VERIFY_IS_TRUE(iter, L"Ensure iterator is still valid");
        VERIFY_ARE_EQUAL(view, (iter++)->Chars(), NoThrowString().Format(L"%s", view.data()));
    }
}

// Function Description:
// - Helper function to validate that the next characters in the buffer at the
//   given location are the provided string. Will return an iterator on the
//   first character after the expectedString.
// Arguments:
// - tb: the buffer who's content we should check
// - expectedString: The characters we're expecting
// - pos: the starting position in the buffer to check the contents of
// Return Value:
// - an iterator on the first character after the expectedString.
TextBufferCellIterator _verifyExpectedString(const TextBuffer& tb,
                                             std::wstring_view expectedString,
                                             const COORD pos)
{
    auto iter = tb.GetCellDataAt(pos);
    _verifyExpectedString(expectedString, iter);
    return iter;
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

    _verifyExpectedString(termTb, L"Hello World ", { 0, 0 });
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
        _verifyExpectedString(tb, L"AAA", { 0, 0 });
        _verifyExpectedString(tb, L"BBB", { 0, 1 });
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
        _verifyExpectedString(tb, L"AAA", { 0, 0 });
        _verifyExpectedString(tb, L"BBB", { 0, 1 });
        _verifyExpectedString(tb, L"   ", { 0, 2 });
        _verifyExpectedString(tb, L"CCC", { 0, 3 });
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

void ConptyRoundtripTests::TestResizeHeight()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        TEST_METHOD_PROPERTY(L"Data:dy", L"{-10, -1, 0, 1, 10}")
        // TEST_METHOD_PROPERTY(L"Data:dy", L"{-1}")
    END_TEST_METHOD_PROPERTIES()
    int dy;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"dy", dy), L"change in height of buffer");

    _checkConptyOutput = false;

    auto& g = ServiceLocator::LocateGlobals();
    auto& renderer = *g.pRender;
    auto& gci = g.getConsoleInformation();
    auto& si = gci.GetActiveOutputBuffer();
    auto& hostSm = si.GetStateMachine();
    auto& hostTb = si.GetTextBuffer();
    auto& termTb = *term->_buffer;
    // auto& termSm = *term->_stateMachine;
    const auto initialHostView = si.GetViewport();
    const auto initialTermView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, initialHostView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, initialHostView.BottomExclusive());
    VERIFY_ARE_EQUAL(0, initialTermView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, initialTermView.BottomExclusive());

    Log::Comment(NoThrowString().Format(
        L"Print 50 lines of output, which will scroll the viewport"));

    for (auto i = 0; i < 50; i++)
    {
        auto wstr = std::wstring(1, static_cast<wchar_t>(L'0' + i));
        hostSm.ProcessString(wstr);
        hostSm.ProcessString(L"\r\n");
    }

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto secondHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, secondHostView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, secondHostView.BottomExclusive());

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    const auto secondTermView = term->GetViewport();
    VERIFY_ARE_EQUAL(50 - initialTermView.Height() + 1, secondTermView.Top());
    VERIFY_ARE_EQUAL(50, secondTermView.BottomInclusive());

    auto verifyTermData = [&termTb]() {
        for (short row = 0; row < 50; row++)
        {
            SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = termTb.GetCellDataAt({ 0, row });
            auto expectedString = std::wstring(1, static_cast<wchar_t>(L'0' + row));

            if (iter->Chars() != expectedString)
            {
                Log::Comment(NoThrowString().Format(L"row [%d] was mismatched", row));
            }
            VERIFY_ARE_EQUAL(expectedString, (iter++)->Chars());
            VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }
    };
    auto verifyHostData = [&hostTb, &si]() {
        const auto hostView = si.GetViewport();
        // The last row of the viewport should be empty
        // The second last row will have '0'+50
        // The third last row will have '0'+49
        // ...
        // The <height> last row will have '0'+(50-height+1)
        const auto firstChar = static_cast<wchar_t>(L'0' + (50 - hostView.Height() + 1));

        // Don't include the last row of the viewport in this check, since it'll be blank
        for (short row = 0; row < hostView.Height() - 1; row++)
        {
            // SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);
            auto iter = hostTb.GetCellDataAt({ 0, row });

            auto expectedString = std::wstring(1, static_cast<wchar_t>(firstChar + row));

            if (iter->Chars() != expectedString)
            {
                Log::Comment(NoThrowString().Format(L"row [%d] was mismatched", row));
            }
            // VERIFY_ARE_EQUAL(expectedString, (iter++)->Chars(), NoThrowString().Format(L"%s", expectedString.data()));
            std::wstring actual{ (iter++)->Chars().data(), 1 };
            Log::Comment(NoThrowString().Format(
                L"Expected, Actual:\"%s\", \"%s\"", expectedString.data(), actual.data()));
            // VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
        }

        // Check the last row of the viewport here
        auto iter = hostTb.GetCellDataAt({ 0, hostView.Height() - 1 });
        // VERIFY_ARE_EQUAL(L" ", (iter)->Chars());
    };
    // verifyData(hostTb);
    verifyHostData();
    verifyTermData();

    const COORD newViewportSize{ TerminalViewWidth, gsl::narrow_cast<short>(TerminalViewHeight + dy) };
    auto resizeResult = term->UserResize(newViewportSize);
    VERIFY_SUCCEEDED(resizeResult);
    _resizeConpty(newViewportSize.X, newViewportSize.Y);

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto thirdHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, thirdHostView.Top());
    VERIFY_ARE_EQUAL(newViewportSize.Y, thirdHostView.BottomExclusive());

    // The Terminal should be stuck on the bottom of the viewport
    const auto thirdTermView = term->GetViewport();

    VERIFY_ARE_EQUAL(50 - thirdTermView.Height() + 1, thirdTermView.Top());
    VERIFY_ARE_EQUAL(50, thirdTermView.BottomInclusive());

    // verifyData(hostTb);
    // verifyData(termTb);
    verifyHostData();
    verifyTermData();

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // Conpty's doesn't have a scrollback, it's view's origin is always 0,0
    const auto fourthHostView = si.GetViewport();
    VERIFY_ARE_EQUAL(0, fourthHostView.Top());
    VERIFY_ARE_EQUAL(newViewportSize.Y, fourthHostView.BottomExclusive());

    // The Terminal should be stuck on the bottom of the viewport
    const auto fourthTermView = term->GetViewport();

    VERIFY_ARE_EQUAL(50 - fourthTermView.Height() + 1, fourthTermView.Top());
    VERIFY_ARE_EQUAL(50, fourthTermView.BottomInclusive());
}
