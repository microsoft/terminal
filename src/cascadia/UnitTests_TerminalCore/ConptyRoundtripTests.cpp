// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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

class ConptyRoundtripTests
{
    TEST_CLASS(ConptyRoundtripTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->InitEvents();
        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();
        m_state->PrepareGlobalInputBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // STEP 1: Set up the Terminal
        term = std::make_unique<Terminal>();
        term->Create({ CommonState::s_csBufferWidth, CommonState::s_csBufferHeight }, 0, emptyRT);

        // STEP 2: Set up the Conpty

        // Set up some sane defaults
        auto& g = ServiceLocator::LocateGlobals();
        auto& gci = g.getConsoleInformation();
        gci.SetDefaultForegroundColor(INVALID_COLOR);
        gci.SetDefaultBackgroundColor(INVALID_COLOR);
        gci.SetFillAttribute(0x07); // DARK_WHITE on DARK_BLACK

        m_state->PrepareNewTextBufferInfo(true);
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
    TEST_METHOD(WriteWrappedLine);

private:
    bool _writeCallback(const char* const pch, size_t const cch);
    void _flushFirstFrame();
    std::deque<std::string> expectedOutput;
    std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
    CommonState* m_state;

    DummyRenderTarget emptyRT;
    std::unique_ptr<Terminal> term;
};

bool ConptyRoundtripTests::_writeCallback(const char* const pch, size_t const cch)
{
    std::string actualString = std::string(pch, cch);
    VERIFY_IS_GREATER_THAN(expectedOutput.size(),
                           static_cast<size_t>(0),
                           NoThrowString().Format(L"writing=\"%hs\", expecting %u strings", actualString.c_str(), expectedOutput.size()));

    std::string first = expectedOutput.front();
    expectedOutput.pop_front();

    Log::Comment(NoThrowString().Format(L"Expected =\t\"%hs\"", first.c_str()));
    Log::Comment(NoThrowString().Format(L"Actual =\t\"%hs\"", actualString.c_str()));

    VERIFY_ARE_EQUAL(first.length(), cch);
    VERIFY_ARE_EQUAL(first, actualString);

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

    auto iter = termTb.GetCellDataAt({ 0, 0 });
    VERIFY_ARE_EQUAL(L"H", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"e", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"l", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"l", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"o", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"W", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"o", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"r", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"l", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L"d", (iter++)->Chars());
    VERIFY_ARE_EQUAL(L" ", (iter++)->Chars());
}

void ConptyRoundtripTests::WriteTwoLinesUsesNewline()
{
    Log::Comment(NoThrowString().Format(
        L"Write two lines of outout. We should use \r\n to move the cursor"));
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

void _verifySpanOfText(const wchar_t* const expectedChar, TextBufferCellIterator& iter, const int start, const int end)
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

void ConptyRoundtripTests::WriteWrappedLine()
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
    const auto view = si.GetViewport();

    _flushFirstFrame();

    const std::wstring aWString(view.Width() - 1, L'A');
    const std::wstring bWString(view.Width() + 1, L'B');

    hostSm.ProcessString(aWString);
    hostSm.ProcessString(L"\n");
    hostSm.ProcessString(bWString);
    hostSm.ProcessString(L"\n");

    Log::Comment(NoThrowString().Format(
        L"Ensure the buffer contains what we'd expect"));
    auto verifyData = [&view](TextBuffer& tb) {
        {
            auto iter = tb.GetCellDataAt({ 0, 0 });
            _verifySpanOfText(L"A", iter, 0, view.Width() - 1);
            VERIFY_ARE_EQUAL(L" ", (iter++)->Chars(), L"The last char of the line should be a space");
        }
        {
            // Every char in this line should be 'B'
            auto iter = tb.GetCellDataAt({ 0, 1 });
            _verifySpanOfText(L"B", iter, 0, view.Width());
        }
        {
            // Only the first char should be 'B', the rest should be blank
            auto iter = tb.GetCellDataAt({ 0, 2 });
            VERIFY_ARE_EQUAL(L"B", (iter++)->Chars());
            _verifySpanOfText(L" ", iter, 1, view.Width());
        }
    };

    verifyData(hostTb);

    std::string aLine(view.Width() - 1, 'A');
    aLine += ' ';
    std::string bLine(view.Width(), 'B');

    // First, the line of 'A's with a space at the end
    expectedOutput.push_back(aLine);
    expectedOutput.push_back("\r\n");
    // Then, the line of all 'B's
    expectedOutput.push_back(bLine);
    // No trailing newline here. Instead, onto the next line, another 'B'
    expectedOutput.push_back("B");
    // Followed by us using ECH to clear the rest of the spaces in the line.
    expectedOutput.push_back("\x1b[K");
    // and finally a newline.
    expectedOutput.push_back("\r\n");

    VERIFY_SUCCEEDED(renderer.PaintFrame());

    // TODO:GH#780 - this test will fail until we implement WriteCharsLegacy2ElectricBoogaloo
    // verifyData(termTb);
}
