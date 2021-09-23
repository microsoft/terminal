// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../renderer/inc/DummyRenderTarget.hpp"
#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "TestUtils.h"

using namespace winrt::Microsoft::Terminal::Core;
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

    TEST_METHOD(TestResetClearTabStops);

    TEST_METHOD(TestAddTabStop);

    TEST_METHOD(TestClearTabStop);

    TEST_METHOD(TestGetForwardTab);

    TEST_METHOD(TestGetReverseTab);

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
    void _SetTabStops(std::list<short> columns, bool replace);
    std::list<short> _GetTabStops();

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
    VERIFY_IS_TRUE(row0.WasWrapForced());

    const auto& row1 = termTb.GetRowByOffset(1);
    VERIFY_IS_FALSE(row1.WasWrapForced());

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
    VERIFY_IS_TRUE(row0.WasWrapForced());

    const auto& row1 = termTb.GetRowByOffset(1);
    VERIFY_IS_FALSE(row1.WasWrapForced());

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

void TerminalBufferTests::_SetTabStops(std::list<short> columns, bool replace)
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    auto& cursor = termTb.GetCursor();

    const auto clearTabStops = L"\033[3g";
    const auto addTabStop = L"\033H";

    if (replace)
    {
        termSm.ProcessString(clearTabStops);
    }

    for (auto column : columns)
    {
        cursor.SetXPosition(column);
        termSm.ProcessString(addTabStop);
    }
}

std::list<short> TerminalBufferTests::_GetTabStops()
{
    std::list<short> columns;
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();
    const auto lastColumn = initialView.RightInclusive();
    auto& cursor = termTb.GetCursor();

    cursor.SetPosition({ 0, 0 });
    for (;;)
    {
        termSm.ProcessCharacter(L'\t');
        auto column = cursor.GetPosition().X;
        if (column >= lastColumn)
        {
            break;
        }
        columns.push_back(column);
    }

    return columns;
}

void TerminalBufferTests::TestResetClearTabStops()
{
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();

    const auto clearTabStops = L"\033[3g";
    const auto resetToInitialState = L"\033c";

    Log::Comment(L"Default tabs every 8 columns.");
    std::list<short> expectedStops{ 8, 16, 24, 32, 40, 48, 56, 64, 72 };
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Clear all tabs.");
    termSm.ProcessString(clearTabStops);
    expectedStops = {};
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"RIS resets tabs to defaults.");
    termSm.ProcessString(resetToInitialState);
    expectedStops = { 8, 16, 24, 32, 40, 48, 56, 64, 72 };
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());
}

void TerminalBufferTests::TestAddTabStop()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    auto& cursor = termTb.GetCursor();

    const auto clearTabStops = L"\033[3g";
    const auto addTabStop = L"\033H";

    Log::Comment(L"Clear all tabs.");
    termSm.ProcessString(clearTabStops);
    std::list<short> expectedStops{};
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Add tab to empty list.");
    cursor.SetXPosition(12);
    termSm.ProcessString(addTabStop);
    expectedStops.push_back(12);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Add tab to head of existing list.");
    cursor.SetXPosition(4);
    termSm.ProcessString(addTabStop);
    expectedStops.push_front(4);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Add tab to tail of existing list.");
    cursor.SetXPosition(30);
    termSm.ProcessString(addTabStop);
    expectedStops.push_back(30);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Add tab to middle of existing list.");
    cursor.SetXPosition(24);
    termSm.ProcessString(addTabStop);
    expectedStops.push_back(24);
    expectedStops.sort();
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());

    Log::Comment(L"Add tab that duplicates an item in the existing list.");
    cursor.SetXPosition(24);
    termSm.ProcessString(addTabStop);
    VERIFY_ARE_EQUAL(expectedStops, _GetTabStops());
}

void TerminalBufferTests::TestClearTabStop()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    auto& cursor = termTb.GetCursor();

    const auto clearTabStops = L"\033[3g";
    const auto clearTabStop = L"\033[0g";
    const auto addTabStop = L"\033H";

    Log::Comment(L"Start with all tabs cleared.");
    {
        termSm.ProcessString(clearTabStops);

        VERIFY_IS_TRUE(_GetTabStops().empty());
    }

    Log::Comment(L"Try to clear nonexistent list.");
    {
        cursor.SetXPosition(0);
        termSm.ProcessString(clearTabStop);

        VERIFY_IS_TRUE(_GetTabStops().empty(), L"List should remain empty");
    }

    Log::Comment(L"Allocate 1 list item and clear it.");
    {
        cursor.SetXPosition(0);
        termSm.ProcessString(addTabStop);
        termSm.ProcessString(clearTabStop);

        VERIFY_IS_TRUE(_GetTabStops().empty());
    }

    Log::Comment(L"Allocate 1 list item and clear nonexistent.");
    {
        cursor.SetXPosition(1);
        termSm.ProcessString(addTabStop);

        Log::Comment(L"Free greater");
        cursor.SetXPosition(2);
        termSm.ProcessString(clearTabStop);
        VERIFY_IS_FALSE(_GetTabStops().empty());

        Log::Comment(L"Free less than");
        cursor.SetXPosition(0);
        termSm.ProcessString(clearTabStop);
        VERIFY_IS_FALSE(_GetTabStops().empty());

        // clear all tab stops
        termSm.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear head.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(inputData, false);
        cursor.SetXPosition(inputData.front());
        termSm.ProcessString(clearTabStop);

        inputData.pop_front();
        VERIFY_ARE_EQUAL(inputData, _GetTabStops());

        // clear all tab stops
        termSm.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear middle.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(inputData, false);
        cursor.SetXPosition(*std::next(inputData.begin()));
        termSm.ProcessString(clearTabStop);

        inputData.erase(std::next(inputData.begin()));
        VERIFY_ARE_EQUAL(inputData, _GetTabStops());

        // clear all tab stops
        termSm.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear tail.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(inputData, false);
        cursor.SetXPosition(inputData.back());
        termSm.ProcessString(clearTabStop);

        inputData.pop_back();
        VERIFY_ARE_EQUAL(inputData, _GetTabStops());

        // clear all tab stops
        termSm.ProcessString(clearTabStops);
    }

    Log::Comment(L"Allocate many (5) list items and clear nonexistent item.");
    {
        std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
        _SetTabStops(inputData, false);
        cursor.SetXPosition(0);
        termSm.ProcessString(clearTabStop);

        VERIFY_ARE_EQUAL(inputData, _GetTabStops());

        // clear all tab stops
        termSm.ProcessString(clearTabStops);
    }
}

void TerminalBufferTests::TestGetForwardTab()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    const auto initialView = term->GetViewport();
    auto& cursor = termTb.GetCursor();

    const auto nextForwardTab = L"\033[I";

    std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
    _SetTabStops(inputData, true);

    const COORD coordScreenBufferSize = initialView.Dimensions();

    Log::Comment(L"Find next tab from before front.");
    {
        cursor.SetXPosition(0);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = inputData.front();

        termSm.ProcessString(nextForwardTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to first tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from in the middle.");
    {
        cursor.SetXPosition(6);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = *std::next(inputData.begin(), 3);

        termSm.ProcessString(nextForwardTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to middle tab stop from sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        cursor.SetXPosition(30);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = coordScreenBufferSize.X - 1;

        termSm.ProcessString(nextForwardTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor advanced to end of screen buffer.");
    }

    Log::Comment(L"Find next tab from rightmost column.");
    {
        cursor.SetXPosition(coordScreenBufferSize.X - 1);

        COORD coordCursorExpected = cursor.GetPosition();

        termSm.ProcessString(nextForwardTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor remains in rightmost column.");
    }
}

void TerminalBufferTests::TestGetReverseTab()
{
    auto& termTb = *term->_buffer;
    auto& termSm = *term->_stateMachine;
    auto& cursor = termTb.GetCursor();

    const auto nextReverseTab = L"\033[Z";

    std::list<short> inputData = { 3, 5, 6, 10, 15, 17 };
    _SetTabStops(inputData, true);

    Log::Comment(L"Find previous tab from before front.");
    {
        cursor.SetXPosition(1);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = 0;

        termSm.ProcessString(nextReverseTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to beginning of the buffer when it started before sample list.");
    }

    Log::Comment(L"Find previous tab from in the middle.");
    {
        cursor.SetXPosition(6);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = *std::next(inputData.begin());

        termSm.ProcessString(nextReverseTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted back one tab spot from middle of sample list.");
    }

    Log::Comment(L"Find next tab from end.");
    {
        cursor.SetXPosition(30);

        COORD coordCursorExpected = cursor.GetPosition();
        coordCursorExpected.X = inputData.back();

        termSm.ProcessString(nextReverseTab);
        COORD const coordCursorResult = cursor.GetPosition();
        VERIFY_ARE_EQUAL(coordCursorExpected,
                         coordCursorResult,
                         L"Cursor adjusted to last item in the sample list from position beyond end.");
    }
}
