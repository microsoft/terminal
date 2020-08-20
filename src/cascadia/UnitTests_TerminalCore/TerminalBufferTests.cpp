// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "TestUtils.h"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace Microsoft::Terminal::Core;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
    class TerminalBufferTests;
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::TerminalBufferTests final
{
    // !!! DANGER: Many tests in this class expect the Terminal buffer
    // to be 80x32. If you change these, you'll probably inadvertently break a
    // bunch of tests !!!
    static const SHORT TerminalViewWidth = 80;
    static const SHORT TerminalViewHeight = 32;
    static const SHORT TerminalHistoryLength = 100;

    TEST_CLASS(TerminalBufferTests);

    TEST_METHOD(TestSimpleBufferWriting);

    TEST_METHOD(TestWrappingCharByChar);
    TEST_METHOD(TestWrappingALongString);

    TEST_METHOD(DontSnapToOutputTest);

    TEST_METHOD_SETUP(MethodSetup)
    {
        // STEP 1: Set up the Terminal
        term = std::make_unique<Terminal>();
        term->Create({ TerminalViewWidth, TerminalViewHeight }, TerminalHistoryLength, emptyRT);
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        term = nullptr;
        return true;
    }

private:
    DummyRenderTarget emptyRT;
    std::unique_ptr<Terminal> term;
};

void TerminalBufferTests::TestSimpleBufferWriting()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(32, initialView.BottomExclusive());

    termSm.ProcessString(L"Hello World");

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, secondView.Top());
    VERIFY_ARE_EQUAL(32, secondView.BottomExclusive());

    TestUtils::VerifyExpectedString(termTb, L"Hello World", { 0, 0 });
}

void TerminalBufferTests::TestWrappingCharByChar()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();
    auto& cursor = termTb.GetCursor();

    const auto charsToWrite = gsl::narrow_cast<short>(TestUtils::Test100CharsString.size());

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(32, initialView.BottomExclusive());

    for (auto i = 0; i < charsToWrite; i++)
    {
        // This is a handy way of just printing the printable characters that
        // _aren't_ the space character.
        const wchar_t wch = static_cast<wchar_t>(33 + (i % 94));
        termSm.ProcessCharacter(wch);
    }

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, secondView.Top());
    VERIFY_ARE_EQUAL(32, secondView.BottomExclusive());

    // Verify the cursor wrapped to the second line
    VERIFY_ARE_EQUAL(charsToWrite % initialView.Width(), cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);

    // Verify that we marked the 0th row as _wrapped_
    const auto& row0 = termTb.GetRowByOffset(0);
    VERIFY_IS_TRUE(row0.GetCharRow().WasWrapForced());

    const auto& row1 = termTb.GetRowByOffset(1);
    VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

    TestUtils::VerifyExpectedString(termTb, TestUtils::Test100CharsString, { 0, 0 });
}

void TerminalBufferTests::TestWrappingALongString()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();
    auto& cursor = termTb.GetCursor();

    const auto charsToWrite = gsl::narrow_cast<short>(TestUtils::Test100CharsString.size());
    VERIFY_ARE_EQUAL(100, charsToWrite);

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(32, initialView.BottomExclusive());

    termSm.ProcessString(TestUtils::Test100CharsString);

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, secondView.Top());
    VERIFY_ARE_EQUAL(32, secondView.BottomExclusive());

    // Verify the cursor wrapped to the second line
    VERIFY_ARE_EQUAL(charsToWrite % initialView.Width(), cursor.GetPosition().X);
    VERIFY_ARE_EQUAL(1, cursor.GetPosition().Y);

    // Verify that we marked the 0th row as _wrapped_
    const auto& row0 = termTb.GetRowByOffset(0);
    VERIFY_IS_TRUE(row0.GetCharRow().WasWrapForced());

    const auto& row1 = termTb.GetRowByOffset(1);
    VERIFY_IS_FALSE(row1.GetCharRow().WasWrapForced());

    TestUtils::VerifyExpectedString(termTb, TestUtils::Test100CharsString, { 0, 0 });
}

void TerminalBufferTests::DontSnapToOutputTest()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();

    VERIFY_ARE_EQUAL(0, initialView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, initialView.BottomExclusive());
    VERIFY_ARE_EQUAL(0, term->_scrollOffset);

    // -1 so that we don't print the last \n
    for (int i = 0; i < TerminalViewHeight + 8 - 1; i++)
    {
        termSm.ProcessString(L"x\n");
    }

    const auto secondView = term->GetViewport();

    VERIFY_ARE_EQUAL(8, secondView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight + 8, secondView.BottomExclusive());
    VERIFY_ARE_EQUAL(0, term->_scrollOffset);

    Log::Comment(L"Scroll up one line");
    term->_scrollOffset = 1;

    const auto thirdView = term->GetViewport();
    VERIFY_ARE_EQUAL(7, thirdView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight + 7, thirdView.BottomExclusive());
    VERIFY_ARE_EQUAL(1, term->_scrollOffset);

    Log::Comment(L"Print a few lines, to see that the viewport stays where it was");
    for (int i = 0; i < 8; i++)
    {
        termSm.ProcessString(L"x\n");
    }

    const auto fourthView = term->GetViewport();
    VERIFY_ARE_EQUAL(7, fourthView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight + 7, fourthView.BottomExclusive());
    VERIFY_ARE_EQUAL(1 + 8, term->_scrollOffset);

    Log::Comment(L"Print enough lines to get the buffer just about ready to "
                 L"circle (on the next newline)");
    auto viewBottom = term->_mutableViewport.BottomInclusive();
    do
    {
        termSm.ProcessString(L"x\n");
        viewBottom = term->_mutableViewport.BottomInclusive();
    } while (viewBottom < termTb.GetSize().BottomInclusive());

    const auto fifthView = term->GetViewport();
    VERIFY_ARE_EQUAL(7, fifthView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight + 7, fifthView.BottomExclusive());
    VERIFY_ARE_EQUAL(TerminalHistoryLength - 7, term->_scrollOffset);

    Log::Comment(L"Print 3 more lines, and see that we stick to where the old "
                 L"rows now are in the buffer (after circling)");
    for (int i = 0; i < 3; i++)
    {
        termSm.ProcessString(L"x\n");
        Log::Comment(NoThrowString().Format(
            L"_scrollOffset: %d", term->_scrollOffset));
    }
    const auto sixthView = term->GetViewport();
    VERIFY_ARE_EQUAL(4, sixthView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight + 4, sixthView.BottomExclusive());
    VERIFY_ARE_EQUAL(TerminalHistoryLength - 4, term->_scrollOffset);

    Log::Comment(L"Print 8 more lines, and see that we're now just stuck at the"
                 L"top of the buffer");
    for (int i = 0; i < 8; i++)
    {
        termSm.ProcessString(L"x\n");
        Log::Comment(NoThrowString().Format(
            L"_scrollOffset: %d", term->_scrollOffset));
    }
    const auto seventhView = term->GetViewport();
    VERIFY_ARE_EQUAL(0, seventhView.Top());
    VERIFY_ARE_EQUAL(TerminalViewHeight, seventhView.BottomExclusive());
    VERIFY_ARE_EQUAL(TerminalHistoryLength, term->_scrollOffset);
}
