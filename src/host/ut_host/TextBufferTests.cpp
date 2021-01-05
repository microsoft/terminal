// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "../buffer/out/textBuffer.hpp"
#include "../buffer/out/CharRow.hpp"

#include "input.h"
#include "_stream.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../renderer/inc/DummyRenderTarget.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class TextBufferTests
{
    DummyRenderTarget _renderTarget;
    CommonState* m_state;

    TEST_CLASS(TextBufferTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state->PrepareNewTextBufferInfo();

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();

        return true;
    }

    TEST_METHOD(TestBufferCreate);

    TextBuffer& GetTbi();

    SHORT GetBufferWidth();

    SHORT GetBufferHeight();

    TEST_METHOD(TestBufferRowByOffset);

    TEST_METHOD(TestWrapFlag);

    TEST_METHOD(TestWrapThroughWriteLine);

    TEST_METHOD(TestDoubleBytePadFlag);

    void DoBoundaryTest(PWCHAR const pwszInputString,
                        short const cLength,
                        short const cMax,
                        short const cLeft,
                        short const cRight);

    TEST_METHOD(TestBoundaryMeasuresRegularString);

    TEST_METHOD(TestBoundaryMeasuresFloatingString);

    TEST_METHOD(TestCopyProperties);

    TEST_METHOD(TestIncrementCursor);

    TEST_METHOD(TestNewlineCursor);

    void TestLastNonSpace(short const cursorPosY);

    TEST_METHOD(TestGetLastNonSpaceCharacter);

    TEST_METHOD(TestSetWrapOnCurrentRow);

    TEST_METHOD(TestIncrementCircularBuffer);

    TEST_METHOD(TestMixedRgbAndLegacyForeground);
    TEST_METHOD(TestMixedRgbAndLegacyBackground);
    TEST_METHOD(TestMixedRgbAndLegacyUnderline);
    TEST_METHOD(TestMixedRgbAndLegacyBrightness);

    TEST_METHOD(TestRgbEraseLine);

    TEST_METHOD(TestUnBold);
    TEST_METHOD(TestUnBoldRgb);
    TEST_METHOD(TestComplexUnBold);

    TEST_METHOD(CopyAttrs);

    TEST_METHOD(EmptySgrTest);

    TEST_METHOD(TestReverseReset);

    TEST_METHOD(CopyLastAttr);

    TEST_METHOD(TestRgbThenBold);
    TEST_METHOD(TestResetClearsBoldness);

    TEST_METHOD(TestBackspaceRightSideVt);

    TEST_METHOD(TestBackspaceStrings);
    TEST_METHOD(TestBackspaceStringsAPI);

    TEST_METHOD(TestRepeatCharacter);

    TEST_METHOD(ResizeTraditional);

    TEST_METHOD(ResizeTraditionalRotationPreservesHighUnicode);
    TEST_METHOD(ScrollBufferRotationPreservesHighUnicode);

    TEST_METHOD(ResizeTraditionalHighUnicodeRowRemoval);
    TEST_METHOD(ResizeTraditionalHighUnicodeColumnRemoval);

    TEST_METHOD(TestBurrito);

    void WriteLinesToBuffer(const std::vector<std::wstring>& text, TextBuffer& buffer);
    TEST_METHOD(GetWordBoundaries);
    TEST_METHOD(MoveByWord);
    TEST_METHOD(GetGlyphBoundaries);

    TEST_METHOD(GetTextRects);
    TEST_METHOD(GetText);

    TEST_METHOD(HyperlinkTrim);
    TEST_METHOD(NoHyperlinkTrim);
};

void TextBufferTests::TestBufferCreate()
{
    VERIFY_SUCCEEDED(m_state->GetTextBufferInfoInitResult());
}

TextBuffer& TextBufferTests::GetTbi()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetTextBuffer();
}

SHORT TextBufferTests::GetBufferWidth()
{
    return GetTbi().GetSize().Width();
}

SHORT TextBufferTests::GetBufferHeight()
{
    return GetTbi().GetSize().Height();
}

void TextBufferTests::TestBufferRowByOffset()
{
    TextBuffer& textBuffer = GetTbi();
    SHORT csBufferHeight = GetBufferHeight();

    VERIFY_IS_TRUE(csBufferHeight > 20);

    short sId = csBufferHeight / 2 - 5;

    const ROW& row = textBuffer.GetRowByOffset(sId);
    VERIFY_ARE_EQUAL(row.GetId(), sId);
}

void TextBufferTests::TestWrapFlag()
{
    TextBuffer& textBuffer = GetTbi();

    ROW& Row = textBuffer._GetFirstRow();

    // no wrap by default
    VERIFY_IS_FALSE(Row.WasWrapForced());

    // try set wrap and check
    Row.SetWrapForced(true);
    VERIFY_IS_TRUE(Row.WasWrapForced());

    // try unset wrap and check
    Row.SetWrapForced(false);
    VERIFY_IS_FALSE(Row.WasWrapForced());
}

void TextBufferTests::TestWrapThroughWriteLine()
{
    TextBuffer& textBuffer = GetTbi();

    auto VerifyWrap = [&](bool expected) {
        ROW& Row = textBuffer._GetFirstRow();

        if (expected)
        {
            VERIFY_IS_TRUE(Row.WasWrapForced());
        }
        else
        {
            VERIFY_IS_FALSE(Row.WasWrapForced());
        }
    };

    // Construct string for testing
    const auto width = textBuffer.GetSize().Width();
    std::wstring chars = L"";
    for (auto i = 0; i < width; i++)
    {
        chars.append(L"a");
    }
    const auto lineOfText = std::move(chars);

    Log::Comment(L"Case 1 : Implicit wrap (false)");
    {
        TextAttribute expectedAttr(FOREGROUND_RED);
        OutputCellIterator it(lineOfText, expectedAttr);

        textBuffer.WriteLine(it, { 0, 0 });
        VerifyWrap(false);
    }

    Log::Comment(L"Case 2 : wrap = true");
    {
        TextAttribute expectedAttr(FOREGROUND_RED);
        OutputCellIterator it(lineOfText, expectedAttr);

        textBuffer.WriteLine(it, { 0, 0 }, true);
        VerifyWrap(true);
    }

    Log::Comment(L"Case 3: wrap = nullopt (remain as TRUE)");
    {
        TextAttribute expectedAttr(FOREGROUND_RED);
        OutputCellIterator it(lineOfText, expectedAttr);

        textBuffer.WriteLine(it, { 0, 0 }, std::nullopt);
        VerifyWrap(true);
    }

    Log::Comment(L"Case 4: wrap = false");
    {
        TextAttribute expectedAttr(FOREGROUND_RED);
        OutputCellIterator it(lineOfText, expectedAttr);

        textBuffer.WriteLine(it, { 0, 0 }, false);
        VerifyWrap(false);
    }

    Log::Comment(L"Case 5: wrap = nullopt (remain as false)");
    {
        TextAttribute expectedAttr(FOREGROUND_RED);
        OutputCellIterator it(lineOfText, expectedAttr);

        textBuffer.WriteLine(it, { 0, 0 }, std::nullopt);
        VerifyWrap(false);
    }
}

void TextBufferTests::TestDoubleBytePadFlag()
{
    TextBuffer& textBuffer = GetTbi();

    ROW& Row = textBuffer._GetFirstRow();

    // no padding by default
    VERIFY_IS_FALSE(Row.WasDoubleBytePadded());

    // try set and check
    Row.SetDoubleBytePadded(true);
    VERIFY_IS_TRUE(Row.WasDoubleBytePadded());

    // try unset and check
    Row.SetDoubleBytePadded(false);
    VERIFY_IS_FALSE(Row.WasDoubleBytePadded());
}

void TextBufferTests::DoBoundaryTest(PWCHAR const pwszInputString,
                                     short const cLength,
                                     short const cMax,
                                     short const cLeft,
                                     short const cRight)
{
    TextBuffer& textBuffer = GetTbi();

    CharRow& charRow = textBuffer._GetFirstRow().GetCharRow();

    // copy string into buffer
    for (size_t i = 0; i < static_cast<size_t>(cLength); ++i)
    {
        charRow.GlyphAt(i) = { &pwszInputString[i], 1 };
    }

    // space pad the rest of the string
    if (cLength < cMax)
    {
        for (short cStart = cLength; cStart < cMax; cStart++)
        {
            charRow.ClearGlyph(cStart);
        }
    }

    // left edge should be 0 since there are no leading spaces
    VERIFY_ARE_EQUAL(charRow.MeasureLeft(), static_cast<size_t>(cLeft));
    // right edge should be one past the index of the last character or the string length
    VERIFY_ARE_EQUAL(charRow.MeasureRight(), static_cast<size_t>(cRight));
}

void TextBufferTests::TestBoundaryMeasuresRegularString()
{
    SHORT csBufferWidth = GetBufferWidth();

    // length 44, left 0, right 44
    const PWCHAR pwszLazyDog = L"The quick brown fox jumps over the lazy dog.";
    DoBoundaryTest(pwszLazyDog, 44, csBufferWidth, 0, 44);
}

void TextBufferTests::TestBoundaryMeasuresFloatingString()
{
    SHORT csBufferWidth = GetBufferWidth();

    // length 5 spaces + 4 chars + 5 spaces = 14, left 5, right 9
    const PWCHAR pwszOffsets = L"     C:\\>     ";
    DoBoundaryTest(pwszOffsets, 14, csBufferWidth, 5, 9);
}

void TextBufferTests::TestCopyProperties()
{
    TextBuffer& otherTbi = GetTbi();

    std::unique_ptr<TextBuffer> testTextBuffer = std::make_unique<TextBuffer>(otherTbi.GetSize().Dimensions(),
                                                                              otherTbi._currentAttributes,
                                                                              12,
                                                                              otherTbi._renderTarget);
    VERIFY_IS_NOT_NULL(testTextBuffer.get());

    // set initial mapping values
    testTextBuffer->GetCursor().SetHasMoved(false);
    otherTbi.GetCursor().SetHasMoved(true);

    testTextBuffer->GetCursor().SetIsVisible(false);
    otherTbi.GetCursor().SetIsVisible(true);

    testTextBuffer->GetCursor().SetIsOn(false);
    otherTbi.GetCursor().SetIsOn(true);

    testTextBuffer->GetCursor().SetIsDouble(false);
    otherTbi.GetCursor().SetIsDouble(true);

    testTextBuffer->GetCursor().SetDelay(false);
    otherTbi.GetCursor().SetDelay(true);

    // run copy
    testTextBuffer->CopyProperties(otherTbi);

    // test that new now contains values from other
    VERIFY_IS_TRUE(testTextBuffer->GetCursor().HasMoved());
    VERIFY_IS_TRUE(testTextBuffer->GetCursor().IsVisible());
    VERIFY_IS_TRUE(testTextBuffer->GetCursor().IsOn());
    VERIFY_IS_TRUE(testTextBuffer->GetCursor().IsDouble());
    VERIFY_IS_TRUE(testTextBuffer->GetCursor().GetDelay());
}

void TextBufferTests::TestIncrementCursor()
{
    TextBuffer& textBuffer = GetTbi();

    // only checking X increments here
    // Y increments are covered in the NewlineCursor test

    short const sBufferWidth = textBuffer.GetSize().Width();

    short const sBufferHeight = textBuffer.GetSize().Height();
    VERIFY_IS_TRUE(sBufferWidth > 1 && sBufferHeight > 1);

    Log::Comment(L"Test normal case of moving once to the right within a single line");
    textBuffer.GetCursor().SetXPosition(0);
    textBuffer.GetCursor().SetYPosition(0);

    COORD coordCursorBefore = textBuffer.GetCursor().GetPosition();

    textBuffer.IncrementCursor();

    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().X, 1); // X should advance by 1
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().Y, coordCursorBefore.Y); // Y shouldn't have moved

    Log::Comment(L"Test line wrap case where cursor is on the right edge of the line");
    textBuffer.GetCursor().SetXPosition(sBufferWidth - 1);
    textBuffer.GetCursor().SetYPosition(0);

    coordCursorBefore = textBuffer.GetCursor().GetPosition();

    textBuffer.IncrementCursor();

    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().X, 0); // position should be reset to the left edge when passing right edge
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().Y - 1, coordCursorBefore.Y); // the cursor should be moved one row down from where it used to be
}

void TextBufferTests::TestNewlineCursor()
{
    TextBuffer& textBuffer = GetTbi();

    const short sBufferHeight = textBuffer.GetSize().Height();

    const short sBufferWidth = textBuffer.GetSize().Width();
    // width and height are sufficiently large for upcoming math
    VERIFY_IS_TRUE(sBufferWidth > 4 && sBufferHeight > 4);

    Log::Comment(L"Verify standard row increment from somewhere in the buffer");

    // set cursor X position to non zero, any position in buffer
    textBuffer.GetCursor().SetXPosition(3);

    // set cursor Y position to not-the-final row in the buffer
    textBuffer.GetCursor().SetYPosition(3);

    COORD coordCursorBefore = textBuffer.GetCursor().GetPosition();

    // perform operation
    textBuffer.NewlineCursor();

    // verify
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().X, 0); // move to left edge of buffer
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().Y, coordCursorBefore.Y + 1); // move down one row

    Log::Comment(L"Verify increment when already on last row of buffer");

    // X position still doesn't matter
    textBuffer.GetCursor().SetXPosition(3);

    // Y position needs to be on the last row of the buffer
    textBuffer.GetCursor().SetYPosition(sBufferHeight - 1);

    coordCursorBefore = textBuffer.GetCursor().GetPosition();

    // perform operation
    textBuffer.NewlineCursor();

    // verify
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().X, 0); // move to left edge
    VERIFY_ARE_EQUAL(textBuffer.GetCursor().GetPosition().Y, coordCursorBefore.Y); // cursor Y position should not have moved. stays on same logical final line of buffer

    // This is okay because the backing circular buffer changes, not the logical screen position (final visible line of the buffer)
}

void TextBufferTests::TestLastNonSpace(short const cursorPosY)
{
    TextBuffer& textBuffer = GetTbi();
    textBuffer.GetCursor().SetYPosition(cursorPosY);

    COORD coordLastNonSpace = textBuffer.GetLastNonSpaceCharacter();

    // We expect the last non space character to be the last printable character in the row.
    // The .Right property on a row is 1 past the last printable character in the row.
    // If there is one character in the row, the last character would be 0.
    // If there are no characters in the row, the last character would be -1 and we need to seek backwards to find the previous row with a character.

    // start expected position from cursor
    COORD coordExpected = textBuffer.GetCursor().GetPosition();

    // Try to get the X position from the current cursor position.
    coordExpected.X = static_cast<short>(textBuffer.GetRowByOffset(coordExpected.Y).GetCharRow().MeasureRight()) - 1;

    // If we went negative, this row was empty and we need to continue seeking upward...
    // - As long as X is negative (empty rows)
    // - As long as we have space before the top of the buffer (Y isn't the 0th/top row).
    while (coordExpected.X < 0 && coordExpected.Y > 0)
    {
        coordExpected.Y--;
        coordExpected.X = static_cast<short>(textBuffer.GetRowByOffset(coordExpected.Y).GetCharRow().MeasureRight()) - 1;
    }

    VERIFY_ARE_EQUAL(coordLastNonSpace.X, coordExpected.X);
    VERIFY_ARE_EQUAL(coordLastNonSpace.Y, coordExpected.Y);
}

void TextBufferTests::TestGetLastNonSpaceCharacter()
{
    m_state->FillTextBuffer(); // fill buffer with some text, it should be 4 rows. See CommonState for details

    Log::Comment(L"Test with cursor inside last row of text");
    TestLastNonSpace(3);

    Log::Comment(L"Test with cursor one beyond last row of text");
    TestLastNonSpace(4);

    Log::Comment(L"Test with cursor way beyond last row of text");
    TestLastNonSpace(14);
}

void TextBufferTests::TestSetWrapOnCurrentRow()
{
    TextBuffer& textBuffer = GetTbi();

    short sCurrentRow = textBuffer.GetCursor().GetPosition().Y;

    ROW& Row = textBuffer.GetRowByOffset(sCurrentRow);

    Log::Comment(L"Testing off to on");

    // turn wrap status off first
    Row.SetWrapForced(false);

    // trigger wrap
    textBuffer._SetWrapOnCurrentRow();

    // ensure this row was flipped
    VERIFY_IS_TRUE(Row.WasWrapForced());

    Log::Comment(L"Testing on stays on");

    // make sure wrap status is on
    Row.SetWrapForced(true);

    // trigger wrap
    textBuffer._SetWrapOnCurrentRow();

    // ensure row is still on
    VERIFY_IS_TRUE(Row.WasWrapForced());
}

void TextBufferTests::TestIncrementCircularBuffer()
{
    TextBuffer& textBuffer = GetTbi();

    short const sBufferHeight = textBuffer.GetSize().Height();

    VERIFY_IS_TRUE(sBufferHeight > 4); // buffer should be sufficiently large

    Log::Comment(L"Test 1 = FirstRow of circular buffer is not the final row of the buffer");
    Log::Comment(L"Test 2 = FirstRow of circular buffer IS THE FINAL ROW of the buffer (and therefore circles)");
    short rgRowsToTest[] = { 2, sBufferHeight - 1 };

    for (UINT iTestIndex = 0; iTestIndex < ARRAYSIZE(rgRowsToTest); iTestIndex++)
    {
        const short iRowToTestIndex = rgRowsToTest[iTestIndex];

        short iNextRowIndex = iRowToTestIndex + 1;
        // if we're at or crossing the height, loop back to 0 (circular buffer)
        if (iNextRowIndex >= sBufferHeight)
        {
            iNextRowIndex = 0;
        }

        textBuffer._firstRow = iRowToTestIndex;

        // fill first row with some stuff
        ROW& FirstRow = textBuffer._GetFirstRow();
        CharRow& charRow = FirstRow.GetCharRow();
        const auto stuff = L'A';
        charRow.GlyphAt(0) = { &stuff, 1 };

        // ensure it does say that it contains text
        VERIFY_IS_TRUE(FirstRow.GetCharRow().ContainsText());

        // try increment
        textBuffer.IncrementCircularBuffer();

        // validate that first row has moved
        VERIFY_ARE_EQUAL(textBuffer._firstRow, iNextRowIndex); // first row has incremented
        VERIFY_ARE_NOT_EQUAL(textBuffer._GetFirstRow(), FirstRow); // the old first row is no longer the first

        // ensure old first row has been emptied
        VERIFY_IS_FALSE(FirstRow.GetCharRow().ContainsText());
    }
}

void TextBufferTests::TestMixedRgbAndLegacyForeground()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    // Case 1 -
    //      Write '\E[m\E[38;2;64;128;255mX\E[49mX\E[m'
    //      Make sure that the second X has RGB attributes (FG and BG)
    //      FG = rgb(64;128;255), BG = rgb(default)
    Log::Comment(L"Case 1 \"\\E[m\\E[38;2;64;128;255mX\\E[49mX\\E[m\"");

    wchar_t* sequence = L"\x1b[m\x1b[38;2;64;128;255mX\x1b[49mX\x1b[m";

    stateMachine.ProcessString(sequence);
    const short x = cursor.GetPosition().X;
    const short y = cursor.GetPosition().Y;
    const ROW& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    const auto fgColor = RGB(64, 128, 255);
    const auto bgColor = gci.LookupAttributeColors(si.GetAttributes()).second;

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(fgColor, bgColor));
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(fgColor, bgColor));

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestMixedRgbAndLegacyBackground()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    // Case 2 -
    //      \E[m\E[48;2;64;128;255mX\E[39mX\E[m
    //      Make sure that the second X has RGB attributes (FG and BG)
    //      FG = rgb(default), BG = rgb(64;128;255)
    Log::Comment(L"Case 2 \"\\E[m\\E[48;2;64;128;255mX\\E[39mX\\E[m\"");

    wchar_t* sequence = L"\x1b[m\x1b[48;2;64;128;255mX\x1b[39mX\x1b[m";
    stateMachine.ProcessString(sequence);
    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    const auto bgColor = RGB(64, 128, 255);
    const auto fgColor = gci.LookupAttributeColors(si.GetAttributes()).first;

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(fgColor, bgColor));
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(fgColor, bgColor));

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestMixedRgbAndLegacyUnderline()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    // Case 3 -
    //      '\E[m\E[48;2;64;128;255mX\E[4mX\E[m'
    //      Make sure that the second X has RGB attributes AND underline
    Log::Comment(L"Case 3 \"\\E[m\\E[48;2;64;128;255mX\\E[4mX\\E[m\"");
    wchar_t* sequence = L"\x1b[m\x1b[48;2;64;128;255mX\x1b[4mX\x1b[m";
    stateMachine.ProcessString(sequence);
    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    const auto bgColor = RGB(64, 128, 255);
    const auto fgColor = gci.LookupAttributeColors(si.GetAttributes()).first;

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(fgColor, bgColor));
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(fgColor, bgColor));

    VERIFY_ARE_EQUAL(attrA.IsUnderlined(), false);
    VERIFY_ARE_EQUAL(attrB.IsUnderlined(), true);

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestMixedRgbAndLegacyBrightness()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();
    // Case 4 -
    //      '\E[m\E[32mX\E[1mX'
    //      Make sure that the second X is a BRIGHT green, not white.
    Log::Comment(L"Case 4 ;\"\\E[m\\E[32mX\\E[1mX\"");
    const auto dark_green = gci.GetColorTableEntry(2);
    const auto bright_green = gci.GetColorTableEntry(10);
    VERIFY_ARE_NOT_EQUAL(dark_green, bright_green);

    wchar_t* sequence = L"\x1b[m\x1b[32mX\x1b[1mX";
    stateMachine.ProcessString(sequence);
    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA).first, dark_green);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB).first, bright_green);

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestRgbEraseLine()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    // Case 1 -
    //      Write '\E[m\E[48;2;64;128;255X\E[48;2;128;128;255\E[KX'
    //      Make sure that all the characters after the first have the rgb attrs
    //      BG = rgb(128;128;255)
    {
        std::wstring sequence = L"\x1b[m\x1b[48;2;64;128;255m";
        stateMachine.ProcessString(sequence);
        sequence = L"X";
        stateMachine.ProcessString(sequence);
        sequence = L"\x1b[48;2;128;128;255m";
        stateMachine.ProcessString(sequence);
        sequence = L"\x1b[K";
        stateMachine.ProcessString(sequence);
        sequence = L"X";
        stateMachine.ProcessString(sequence);

        const auto x = cursor.GetPosition().X;
        const auto y = cursor.GetPosition().Y;

        Log::Comment(NoThrowString().Format(
            L"cursor={X:%d,Y:%d}",
            x,
            y));
        VERIFY_ARE_EQUAL(x, 2);
        VERIFY_ARE_EQUAL(y, 0);

        const auto& row = tbi.GetRowByOffset(y);
        const auto attrRow = &row.GetAttrRow();
        const auto len = tbi.GetSize().Width();
        const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };

        const auto attr0 = attrs[0];

        VERIFY_ARE_EQUAL(attr0.IsLegacy(), false);
        VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr0).second, RGB(64, 128, 255));

        for (auto i = 1; i < len; i++)
        {
            const auto attr = attrs[i];
            LOG_ATTR(attr);
            VERIFY_ARE_EQUAL(attr.IsLegacy(), false);
            VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr).second, RGB(128, 128, 255));
        }
        std::wstring reset = L"\x1b[0m";
        stateMachine.ProcessString(reset);
    }
}

void TextBufferTests::TestUnBold()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    // Case 1 -
    //      Write '\E[1;32mX\E[22mX'
    //      The first X should be bright green.
    //      The second x should be dark green.
    std::wstring sequence = L"\x1b[1;32mX\x1b[22mX";
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto dark_green = gci.GetColorTableEntry(2);
    const auto bright_green = gci.GetColorTableEntry(10);

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_ARE_EQUAL(x, 2);
    VERIFY_ARE_EQUAL(y, 0);

    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA).first, bright_green);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB).first, dark_green);

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestUnBoldRgb()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    // Case 2 -
    //      Write '\E[1;32m\E[48;2;1;2;3mX\E[22mX'
    //      The first X should be bright green, and not legacy.
    //      The second X should be dark green, and not legacy.
    //      BG = rgb(1;2;3)
    std::wstring sequence = L"\x1b[1;32m\x1b[48;2;1;2;3mX\x1b[22mX";
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto dark_green = gci.GetColorTableEntry(2);
    const auto bright_green = gci.GetColorTableEntry(10);

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_ARE_EQUAL(x, 2);
    VERIFY_ARE_EQUAL(y, 0);

    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA).first, bright_green);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB).first, dark_green);

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestComplexUnBold()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    // Case 3 -
    //      Write '\E[1;32m\E[48;2;1;2;3mA\E[22mB\E[38;2;32;32;32mC\E[1mD\E[38;2;64;64;64mE\E[22mF'
    //      The A should be bright green, and not legacy.
    //      The B should be dark green, and not legacy.
    //      The C should be rgb(32, 32, 32), and not legacy.
    //      The D should be unchanged from the third.
    //      The E should be rgb(64, 64, 64), and not legacy.
    //      The F should be rgb(64, 64, 64), and not legacy.
    //      BG = rgb(1;2;3)
    std::wstring sequence = L"\x1b[1;32m\x1b[48;2;1;2;3mA\x1b[22mB\x1b[38;2;32;32;32mC\x1b[1mD\x1b[38;2;64;64;64mE\x1b[22mF";
    Log::Comment(NoThrowString().Format(sequence.c_str()));
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto dark_green = gci.GetColorTableEntry(2);
    const auto bright_green = gci.GetColorTableEntry(10);

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_ARE_EQUAL(x, 6);
    VERIFY_ARE_EQUAL(y, 0);

    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 6];
    const auto attrB = attrs[x - 5];
    const auto attrC = attrs[x - 4];
    const auto attrD = attrs[x - 3];
    const auto attrE = attrs[x - 2];
    const auto attrF = attrs[x - 1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    Log::Comment(NoThrowString().Format(
        L"attrA=%s", VerifyOutputTraits<TextAttribute>::ToString(attrA).GetBuffer()));
    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);
    LOG_ATTR(attrD);
    LOG_ATTR(attrE);
    LOG_ATTR(attrF);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrC.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrD.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrE.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrF.IsLegacy(), false);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(bright_green, RGB(1, 2, 3)));
    VERIFY_IS_TRUE(attrA.IsBold());

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(dark_green, RGB(1, 2, 3)));
    VERIFY_IS_FALSE(attrB.IsBold());

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrC), std::make_pair(RGB(32, 32, 32), RGB(1, 2, 3)));
    VERIFY_IS_FALSE(attrC.IsBold());

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrD), gci.LookupAttributeColors(attrC));
    VERIFY_IS_TRUE(attrD.IsBold());

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrE), std::make_pair(RGB(64, 64, 64), RGB(1, 2, 3)));
    VERIFY_IS_TRUE(attrE.IsBold());

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrF), std::make_pair(RGB(64, 64, 64), RGB(1, 2, 3)));
    VERIFY_IS_FALSE(attrF.IsBold());

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::CopyAttrs()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();
    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);
    // Write '\E[32mX\E[33mX\n\E[34mX\E[35mX\E[H\E[M'
    // The first two X's should get deleted.
    // The third X should be blue
    // The fourth X should be magenta
    std::wstring sequence = L"\x1b[32mX\x1b[33mX\n\x1b[34mX\x1b[35mX\x1b[H\x1b[M";
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto dark_blue = gci.GetColorTableEntry(1);
    const auto dark_magenta = gci.GetColorTableEntry(5);

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_ARE_EQUAL(x, 0);
    VERIFY_ARE_EQUAL(y, 0);

    const auto& row = tbi.GetRowByOffset(0);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[0];
    const auto attrB = attrs[1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA).first, dark_blue);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB).first, dark_magenta);
}

void TextBufferTests::EmptySgrTest()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();

    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
    const auto [defaultFg, defaultBg] = gci.LookupAttributeColors(si.GetAttributes());

    // Case 1 -
    //      Write '\x1b[0mX\x1b[31mX\x1b[31;m'
    //      The first X should be default colors.
    //      The second X should be (darkRed,default).
    //      The third X should be default colors.
    std::wstring sequence = L"\x1b[0mX\x1b[31mX\x1b[31;mX";
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const COLORREF darkRed = gci.GetColorTableEntry(4);
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_IS_TRUE(x >= 3);

    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 3];
    const auto attrB = attrs[x - 2];
    const auto attrC = attrs[x - 1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(defaultFg, defaultBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(darkRed, defaultBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrC), std::make_pair(defaultFg, defaultBg));

    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestReverseReset()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();

    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
    const auto [defaultFg, defaultBg] = gci.LookupAttributeColors(si.GetAttributes());

    // Case 1 -
    //      Write '\E[42m\E[38;2;128;5;255mX\E[7mX\E[27mX'
    //      The first X should be (fg,bg) = (rgb(128;5;255), dark_green)
    //      The second X should be (fg,bg) = (dark_green, rgb(128;5;255))
    //      The third X should be (fg,bg) = (rgb(128;5;255), dark_green)
    std::wstring sequence = L"\x1b[42m\x1b[38;2;128;5;255mX\x1b[7mX\x1b[27mX";
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto dark_green = gci.GetColorTableEntry(2);
    const COLORREF rgbColor = RGB(128, 5, 255);

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    VERIFY_IS_TRUE(x >= 3);

    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const auto len = tbi.GetSize().Width();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 3];
    const auto attrB = attrs[x - 2];
    const auto attrC = attrs[x - 1];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(rgbColor, dark_green));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(dark_green, rgbColor));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrC), std::make_pair(rgbColor, dark_green));

    stateMachine.ProcessString(reset);
}

void TextBufferTests::CopyLastAttr()
{
    DisableVerifyExceptions disable;

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();

    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    std::wstring reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
    const auto [defaultFg, defaultBg] = gci.LookupAttributeColors(si.GetAttributes());

    const COLORREF solFg = RGB(101, 123, 131);
    const COLORREF solBg = RGB(0, 43, 54);
    const COLORREF solCyan = RGB(42, 161, 152);

    std::wstring solFgSeq = L"\x1b[38;2;101;123;131m";
    std::wstring solBgSeq = L"\x1b[48;2;0;43;54m";
    std::wstring solCyanSeq = L"\x1b[38;2;42;161;152m";

    // Make sure that the color table has certain values we expect
    const COLORREF defaultBrightBlack = RGB(118, 118, 118);
    const COLORREF defaultBrightYellow = RGB(249, 241, 165);
    const COLORREF defaultBrightCyan = RGB(97, 214, 214);

    gci.SetColorTableEntry(8, defaultBrightBlack);
    gci.SetColorTableEntry(14, defaultBrightYellow);
    gci.SetColorTableEntry(11, defaultBrightCyan);

    // Write (solFg, solBG) X \n
    //       (solFg, solBG) X (solCyan, solBG) X \n
    //       (solFg, solBG) X (solCyan, solBG) X (solFg, solBG) X
    // then go home, and insert a line.

    // Row 1
    stateMachine.ProcessString(solFgSeq);
    stateMachine.ProcessString(solBgSeq);
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\n");

    // Row 2
    // Remember that the colors from before persist here too, so we don't need
    //      to emit both the FG and BG if they haven't changed.
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(solCyanSeq);
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(L"\n");

    // Row 3
    stateMachine.ProcessString(solFgSeq);
    stateMachine.ProcessString(solBgSeq);
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(solCyanSeq);
    stateMachine.ProcessString(L"X");
    stateMachine.ProcessString(solFgSeq);
    stateMachine.ProcessString(L"X");

    std::wstring insertLineAtHome = L"\x1b[H\x1b[L";
    stateMachine.ProcessString(insertLineAtHome);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    const ROW& row1 = tbi.GetRowByOffset(y + 1);
    const ROW& row2 = tbi.GetRowByOffset(y + 2);
    const ROW& row3 = tbi.GetRowByOffset(y + 3);
    const auto len = tbi.GetSize().Width();

    const std::vector<TextAttribute> attrs1{ row1.GetAttrRow().begin(), row1.GetAttrRow().end() };
    const std::vector<TextAttribute> attrs2{ row2.GetAttrRow().begin(), row2.GetAttrRow().end() };
    const std::vector<TextAttribute> attrs3{ row3.GetAttrRow().begin(), row3.GetAttrRow().end() };

    const auto attr1A = attrs1[0];

    const auto attr2A = attrs2[0];
    const auto attr2B = attrs2[1];

    const auto attr3A = attrs3[0];
    const auto attr3B = attrs3[1];
    const auto attr3C = attrs3[2];

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));

    LOG_ATTR(attr1A);
    LOG_ATTR(attr2A);
    LOG_ATTR(attr2A);
    LOG_ATTR(attr3A);
    LOG_ATTR(attr3B);
    LOG_ATTR(attr3C);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr1A), std::make_pair(solFg, solBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr2A), std::make_pair(solFg, solBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr2B), std::make_pair(solCyan, solBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr3A), std::make_pair(solFg, solBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr3B), std::make_pair(solCyan, solBg));

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attr3C), std::make_pair(solFg, solBg));

    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestRgbThenBold()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();
    // See MSFT:16398982
    Log::Comment(NoThrowString().Format(
        L"Test that a bold following a RGB color doesn't remove the RGB color"));
    Log::Comment(L"\"\\x1b[38;2;40;40;40m\\x1b[48;2;168;153;132mX\\x1b[1mX\\x1b[m\"");
    const auto foreground = RGB(40, 40, 40);
    const auto background = RGB(168, 153, 132);

    const wchar_t* const sequence = L"\x1b[38;2;40;40;40m\x1b[48;2;168;153;132mX\x1b[1mX\x1b[m";
    stateMachine.ProcessString(sequence);
    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x - 2];
    const auto attrB = attrs[x - 1];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    Log::Comment(NoThrowString().Format(
        L"attrA should be RGB, and attrB should be the same as attrA, NOT bolded"));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);

    VERIFY_ARE_EQUAL(attrA.IsLegacy(), false);
    VERIFY_ARE_EQUAL(attrB.IsLegacy(), false);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA), std::make_pair(foreground, background));
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB), std::make_pair(foreground, background));

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestResetClearsBoldness()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();
    Log::Comment(NoThrowString().Format(
        L"Test that resetting bold attributes clears the boldness."));
    const auto x0 = cursor.GetPosition().X;

    // Test assumes that the background/foreground were default attribute when it starts up,
    // so set that here.
    TextAttribute defaultAttribute;
    si.SetAttributes(defaultAttribute);

    const auto [defaultFg, defaultBg] = gci.LookupAttributeColors(si.GetAttributes());
    const auto dark_green = gci.GetColorTableEntry(2);
    const auto bright_green = gci.GetColorTableEntry(10);

    wchar_t* sequence = L"\x1b[32mA\x1b[1mB\x1b[0mC\x1b[32mD";
    Log::Comment(NoThrowString().Format(sequence));
    stateMachine.ProcessString(sequence);

    const auto x = cursor.GetPosition().X;
    const auto y = cursor.GetPosition().Y;
    const auto& row = tbi.GetRowByOffset(y);
    const auto attrRow = &row.GetAttrRow();
    const std::vector<TextAttribute> attrs{ attrRow->begin(), attrRow->end() };
    const auto attrA = attrs[x0];
    const auto attrB = attrs[x0 + 1];
    const auto attrC = attrs[x0 + 2];
    const auto attrD = attrs[x0 + 3];
    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x,
        y));
    Log::Comment(NoThrowString().Format(
        L"attrA should be RGB, and attrB should be the same as attrA, NOT bolded"));

    LOG_ATTR(attrA);
    LOG_ATTR(attrB);
    LOG_ATTR(attrC);
    LOG_ATTR(attrD);

    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrA).first, dark_green);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrB).first, bright_green);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrC).first, defaultFg);
    VERIFY_ARE_EQUAL(gci.LookupAttributeColors(attrD).first, dark_green);

    VERIFY_IS_FALSE(attrA.IsBold());
    VERIFY_IS_TRUE(attrB.IsBold());
    VERIFY_IS_FALSE(attrC.IsBold());
    VERIFY_IS_FALSE(attrD.IsBold());

    wchar_t* reset = L"\x1b[0m";
    stateMachine.ProcessString(reset);
}

void TextBufferTests::TestBackspaceRightSideVt()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    Log::Comment(L"verify that backspace has the same behavior as a vt CUB sequence once "
                 L"we've traversed to the right side of the current row");

    const wchar_t* const sequence = L"\033[1000Cx\by\n";
    Log::Comment(NoThrowString().Format(sequence));

    const auto preCursorPosition = cursor.GetPosition();
    stateMachine.ProcessString(sequence);
    const auto postCursorPosition = cursor.GetPosition();

    // make sure newline was handled correctly
    VERIFY_ARE_EQUAL(0, postCursorPosition.X);
    VERIFY_ARE_EQUAL(preCursorPosition.Y, postCursorPosition.Y - 1);

    // make sure "yx" was written to the end of the line the cursor started on
    const auto& row = tbi.GetRowByOffset(preCursorPosition.Y);
    const auto rowText = row.GetText();
    auto it = rowText.crbegin();
    VERIFY_ARE_EQUAL(*it, L'x');
    ++it;
    VERIFY_ARE_EQUAL(*it, L'y');
}

void TextBufferTests::TestBackspaceStrings()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    const Cursor& cursor = tbi.GetCursor();

    const auto x0 = cursor.GetPosition().X;
    const auto y0 = cursor.GetPosition().Y;

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x0,
        y0));
    std::wstring seq = L"a\b \b";
    stateMachine.ProcessString(seq);

    const auto x1 = cursor.GetPosition().X;
    const auto y1 = cursor.GetPosition().Y;

    VERIFY_ARE_EQUAL(x1, x0);
    VERIFY_ARE_EQUAL(y1, y0);

    seq = L"a";
    stateMachine.ProcessString(seq);
    seq = L"\b";
    stateMachine.ProcessString(seq);
    seq = L" ";
    stateMachine.ProcessString(seq);
    seq = L"\b";
    stateMachine.ProcessString(seq);

    const auto x2 = cursor.GetPosition().X;
    const auto y2 = cursor.GetPosition().Y;

    VERIFY_ARE_EQUAL(x2, x0);
    VERIFY_ARE_EQUAL(y2, y0);
}

void TextBufferTests::TestBackspaceStringsAPI()
{
    // Pretty much the same as the above test, but explicitly DOESN'T use the
    //  state machine.
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    const TextBuffer& tbi = si.GetTextBuffer();
    const Cursor& cursor = tbi.GetCursor();

    gci.SetVirtTermLevel(0);
    WI_ClearFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const auto x0 = cursor.GetPosition().X;
    const auto y0 = cursor.GetPosition().Y;

    Log::Comment(NoThrowString().Format(
        L"cursor={X:%d,Y:%d}",
        x0,
        y0));

    // We're going to write an "a" to the buffer in various ways, then try
    //      backspacing it with "\b \b".
    // Regardless of how we write those sequences of characters, the end result
    //      should be the same.
    std::unique_ptr<WriteData> waiter;

    size_t aCb = 2;
    VERIFY_SUCCEEDED(DoWriteConsole(L"a", &aCb, si, false, waiter));

    size_t seqCb = 6;
    Log::Comment(NoThrowString().Format(
        L"Using WriteCharsLegacy, write \\b \\b as a single string."));
    {
        wchar_t* str = L"\b \b";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, 0, nullptr));

        VERIFY_ARE_EQUAL(cursor.GetPosition().X, x0);
        VERIFY_ARE_EQUAL(cursor.GetPosition().Y, y0);

        Log::Comment(NoThrowString().Format(
            L"Using DoWriteConsole, write \\b \\b as a single string."));
        VERIFY_SUCCEEDED(DoWriteConsole(L"a", &aCb, si, false, waiter));

        VERIFY_SUCCEEDED(DoWriteConsole(str, &seqCb, si, false, waiter));
        VERIFY_ARE_EQUAL(cursor.GetPosition().X, x0);
        VERIFY_ARE_EQUAL(cursor.GetPosition().Y, y0);
    }

    seqCb = 2;

    Log::Comment(NoThrowString().Format(
        L"Using DoWriteConsole, write \\b \\b as separate strings."));

    VERIFY_SUCCEEDED(DoWriteConsole(L"a", &seqCb, si, false, waiter));
    VERIFY_SUCCEEDED(DoWriteConsole(L"\b", &seqCb, si, false, waiter));
    VERIFY_SUCCEEDED(DoWriteConsole(L" ", &seqCb, si, false, waiter));
    VERIFY_SUCCEEDED(DoWriteConsole(L"\b", &seqCb, si, false, waiter));

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, x0);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, y0);

    Log::Comment(NoThrowString().Format(
        L"Using WriteCharsLegacy, write \\b \\b as separate strings."));
    {
        wchar_t* str = L"a";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, 0, nullptr));
    }
    {
        wchar_t* str = L"\b";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, 0, nullptr));
    }
    {
        wchar_t* str = L" ";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, 0, nullptr));
    }
    {
        wchar_t* str = L"\b";
        VERIFY_SUCCESS_NTSTATUS(WriteCharsLegacy(si, str, str, str, &seqCb, nullptr, cursor.GetPosition().X, 0, nullptr));
    }

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, x0);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, y0);
}

void TextBufferTests::TestRepeatCharacter()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& si = gci.GetActiveOutputBuffer().GetActiveBuffer();
    TextBuffer& tbi = si.GetTextBuffer();
    StateMachine& stateMachine = si.GetStateMachine();
    Cursor& cursor = tbi.GetCursor();

    WI_SetFlag(si.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cursor.SetXPosition(0);
    cursor.SetYPosition(0);

    Log::Comment(
        L"Test 0: Simply repeat a single character.");

    std::wstring sequence = L"X";
    stateMachine.ProcessString(sequence);

    sequence = L"\x1b[b";
    stateMachine.ProcessString(sequence);

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 2);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 0);

    {
        const auto& row0 = tbi.GetRowByOffset(0);
        const auto row0Text = row0.GetText();
        VERIFY_ARE_EQUAL(L'X', row0Text[0]);
        VERIFY_ARE_EQUAL(L'X', row0Text[1]);
        VERIFY_ARE_EQUAL(L' ', row0Text[2]);
    }

    Log::Comment(
        L"Test 1: Try repeating characters after another VT action. It should do nothing.");

    stateMachine.ProcessString(L"\n");
    stateMachine.ProcessString(L"A");
    stateMachine.ProcessString(L"B");
    stateMachine.ProcessString(L"\x1b[A");
    stateMachine.ProcessString(L"\x1b[b");

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 2);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 0);

    {
        const auto& row0 = tbi.GetRowByOffset(0);
        const auto& row1 = tbi.GetRowByOffset(1);
        const auto row0Text = row0.GetText();
        const auto row1Text = row1.GetText();
        VERIFY_ARE_EQUAL(L'X', row0Text[0]);
        VERIFY_ARE_EQUAL(L'X', row0Text[1]);
        VERIFY_ARE_EQUAL(L' ', row0Text[2]);
        VERIFY_ARE_EQUAL(L'A', row1Text[0]);
        VERIFY_ARE_EQUAL(L'B', row1Text[1]);
        VERIFY_ARE_EQUAL(L' ', row1Text[2]);
    }

    Log::Comment(
        L"Test 2: Repeat a character lots of times");

    stateMachine.ProcessString(L"\x1b[3;H");
    stateMachine.ProcessString(L"C");
    stateMachine.ProcessString(L"\x1b[5b");

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 6);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 2);

    {
        const auto& row2 = tbi.GetRowByOffset(2);
        const auto row2Text = row2.GetText();
        VERIFY_ARE_EQUAL(L'C', row2Text[0]);
        VERIFY_ARE_EQUAL(L'C', row2Text[1]);
        VERIFY_ARE_EQUAL(L'C', row2Text[2]);
        VERIFY_ARE_EQUAL(L'C', row2Text[3]);
        VERIFY_ARE_EQUAL(L'C', row2Text[4]);
        VERIFY_ARE_EQUAL(L'C', row2Text[5]);
        VERIFY_ARE_EQUAL(L' ', row2Text[6]);
    }

    Log::Comment(
        L"Test 3: try repeating a non-graphical character. It should do nothing.");

    stateMachine.ProcessString(L"\r\n");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 0);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 3);
    stateMachine.ProcessString(L"D\n");
    stateMachine.ProcessString(L"\x1b[b");

    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 0);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 4);

    Log::Comment(
        L"Test 4: try repeating multiple times. It should do nothing.");

    stateMachine.ProcessString(L"\r\n");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 0);
    VERIFY_ARE_EQUAL(cursor.GetPosition().Y, 5);
    stateMachine.ProcessString(L"E");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 1);
    stateMachine.ProcessString(L"\x1b[b");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 2);
    stateMachine.ProcessString(L"\x1b[b");
    VERIFY_ARE_EQUAL(cursor.GetPosition().X, 2);

    {
        const auto& row5 = tbi.GetRowByOffset(5);
        const auto row5Text = row5.GetText();
        VERIFY_ARE_EQUAL(L'E', row5Text[0]);
        VERIFY_ARE_EQUAL(L'E', row5Text[1]);
        VERIFY_ARE_EQUAL(L' ', row5Text[2]);
    }
}

void TextBufferTests::ResizeTraditional()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:shrinkX", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:shrinkY", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool shrinkX;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"shrinkX", shrinkX), L"Shrink X = true, Grow X = false");

    bool shrinkY;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"shrinkY", shrinkY), L"Shrink Y = true, Grow Y = false");

    const COORD smallSize = { 5, 5 };
    const TextAttribute defaultAttr(0);

    TextBuffer buffer(smallSize, defaultAttr, 12, _renderTarget);

    Log::Comment(L"Fill buffer with some data and do assorted resize operations.");

    wchar_t expectedChar = L'A';
    const std::wstring_view expectedView(&expectedChar, 1);
    TextAttribute expectedAttr(FOREGROUND_RED);
    OutputCellIterator it(expectedChar, expectedAttr);
    const auto finalIt = buffer.Write(it);
    VERIFY_ARE_EQUAL(smallSize.X * smallSize.Y, finalIt.GetCellDistance(it), L"Verify we said we filled every cell.");

    const Viewport writtenView = Viewport::FromDimensions({ 0, 0 }, smallSize);

    Log::Comment(L"Ensure every cell has our test pattern value.");
    {
        TextBufferCellIterator viewIt(buffer, { 0, 0 });
        while (viewIt)
        {
            VERIFY_ARE_EQUAL(expectedView, viewIt->Chars());
            VERIFY_ARE_EQUAL(expectedAttr, viewIt->TextAttr());
            viewIt++;
        }
    }

    Log::Comment(L"Resize to X and Y.");
    COORD newSize = smallSize;

    if (shrinkX)
    {
        newSize.X -= 2;
    }
    else
    {
        newSize.X += 2;
    }

    if (shrinkY)
    {
        newSize.Y -= 2;
    }
    else
    {
        newSize.Y += 2;
    }

    // When we grow, we extend the last color. Therefore, this region covers the area colored the same as the letters but filled with a blank.
    const auto widthAdjustedView = Viewport::FromDimensions(writtenView.Origin(), { newSize.X, smallSize.Y });

    // When we resize, we expect the attributes to be unchanged, but the new cells
    //  to be filled with spaces
    wchar_t expectedSpace = UNICODE_SPACE;
    std::wstring_view expectedSpaceView(&expectedSpace, 1);

    VERIFY_SUCCEEDED(buffer.ResizeTraditional(newSize));

    Log::Comment(L"Verify every cell in the X dimension is still the same as when filled and the new Y row is just empty default cells.");
    {
        TextBufferCellIterator viewIt(buffer, { 0, 0 });
        while (viewIt)
        {
            Log::Comment(NoThrowString().Format(L"Checking cell (Y=%d, X=%d)", viewIt._pos.Y, viewIt._pos.X));
            if (writtenView.IsInBounds(viewIt._pos))
            {
                Log::Comment(L"This position is inside our original write area. It should have the original character and color.");
                // If the position is in bounds with what we originally wrote, it should have that character and color.
                VERIFY_ARE_EQUAL(expectedView, viewIt->Chars());
                VERIFY_ARE_EQUAL(expectedAttr, viewIt->TextAttr());
            }
            else if (widthAdjustedView.IsInBounds(viewIt._pos))
            {
                Log::Comment(L"This position is right of our original write area. It should have extended the color rightward and filled with a space.");
                // If we missed the original fill, but we're still in region defined by the adjusted width, then
                // the color was extended outward but without the character value.
                VERIFY_ARE_EQUAL(expectedSpaceView, viewIt->Chars());
                VERIFY_ARE_EQUAL(expectedAttr, viewIt->TextAttr());
            }
            else
            {
                Log::Comment(L"This position is below our original write area. It should have filled blank lines (space lines) with the default fill color.");
                // Otherwise, we use the default.
                VERIFY_ARE_EQUAL(expectedSpaceView, viewIt->Chars());
                VERIFY_ARE_EQUAL(defaultAttr, viewIt->TextAttr());
            }
            viewIt++;
        }
    }
}

// This tests that when buffer storage rows are rotated around during a resize traditional operation,
// that the Unicode Storage-held high unicode items like emoji rotate properly with it.
void TextBufferTests::ResizeTraditionalRotationPreservesHighUnicode()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Get a position inside the buffer
    const COORD pos{ 2, 1 };
    auto position = _buffer->_storage[pos.Y].GetCharRow().GlyphAt(pos.X);

    // Fill it up with a sequence that will have to hit the high unicode storage.
    // This is the negative squared latin capital letter B emoji: 🅱
    // It's encoded in UTF-16, as needed by the buffer.
    const auto bButton = L"\xD83C\xDD71";
    position = bButton;

    // Read back the text at that position and ensure that it matches what we wrote.
    const auto readBack = _buffer->GetTextDataAt(pos);
    const auto readBackText = *readBack;
    VERIFY_ARE_EQUAL(String(bButton), String(readBackText.data(), gsl::narrow<int>(readBackText.size())));

    // Make it the first row in the buffer so it will rotate around when we resize and cause renumbering
    const SHORT delta = _buffer->GetFirstRowIndex() - pos.Y;
    const COORD newPos{ pos.X, pos.Y + delta };

    _buffer->_SetFirstRowIndex(pos.Y);

    // Perform resize to rotate the rows around
    VERIFY_NT_SUCCESS(_buffer->ResizeTraditional(bufferSize));

    // Retrieve the text at the old and new positions.
    const auto shouldBeEmptyText = *_buffer->GetTextDataAt(pos);
    const auto shouldBeEmojiText = *_buffer->GetTextDataAt(newPos);

    VERIFY_ARE_EQUAL(String(L" "), String(shouldBeEmptyText.data(), gsl::narrow<int>(shouldBeEmptyText.size())));
    VERIFY_ARE_EQUAL(String(bButton), String(shouldBeEmojiText.data(), gsl::narrow<int>(shouldBeEmojiText.size())));
}

// This tests that when buffer storage rows are rotated around during a scroll buffer operation,
// that the Unicode Storage-held high unicode items like emoji rotate properly with it.
void TextBufferTests::ScrollBufferRotationPreservesHighUnicode()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Get a position inside the buffer
    const COORD pos{ 2, 1 };
    auto position = _buffer->_storage[pos.Y].GetCharRow().GlyphAt(pos.X);

    // Fill it up with a sequence that will have to hit the high unicode storage.
    // This is the fire emoji: 🔥
    // It's encoded in UTF-16, as needed by the buffer.
    const auto fire = L"\xD83D\xDD25";
    position = fire;

    // Read back the text at that position and ensure that it matches what we wrote.
    const auto readBack = _buffer->GetTextDataAt(pos);
    const auto readBackText = *readBack;
    VERIFY_ARE_EQUAL(String(fire), String(readBackText.data(), gsl::narrow<int>(readBackText.size())));

    // Prepare a delta and the new position we expect the symbol to be moved into.
    const SHORT delta = 5;
    const COORD newPos{ pos.X, pos.Y + delta };

    // Scroll the row with our data by delta.
    _buffer->ScrollRows(pos.Y, 1, delta);

    // Retrieve the text at the old and new positions.
    const auto shouldBeEmptyText = *_buffer->GetTextDataAt(pos);
    const auto shouldBeFireText = *_buffer->GetTextDataAt(newPos);

    VERIFY_ARE_EQUAL(String(L" "), String(shouldBeEmptyText.data(), gsl::narrow<int>(shouldBeEmptyText.size())));
    VERIFY_ARE_EQUAL(String(fire), String(shouldBeFireText.data(), gsl::narrow<int>(shouldBeFireText.size())));
}

// This tests that rows removed from the buffer while resizing traditionally will also drop the high unicode
// characters from the Unicode Storage buffer
void TextBufferTests::ResizeTraditionalHighUnicodeRowRemoval()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Get a position inside the buffer in the bottom row
    const COORD pos{ 0, bufferSize.Y - 1 };
    auto position = _buffer->_storage[pos.Y].GetCharRow().GlyphAt(pos.X);

    // Fill it up with a sequence that will have to hit the high unicode storage.
    // This is the eggplant emoji: 🍆
    // It's encoded in UTF-16, as needed by the buffer.
    const auto emoji = L"\xD83C\xDF46";
    position = emoji;

    // Read back the text at that position and ensure that it matches what we wrote.
    const auto readBack = _buffer->GetTextDataAt(pos);
    const auto readBackText = *readBack;
    VERIFY_ARE_EQUAL(String(emoji), String(readBackText.data(), gsl::narrow<int>(readBackText.size())));

    VERIFY_ARE_EQUAL(1u, _buffer->GetUnicodeStorage()._map.size(), L"There should be one item in the map.");

    // Perform resize to trim off the row of the buffer that included the emoji
    COORD trimmedBufferSize{ bufferSize.X, bufferSize.Y - 1 };

    VERIFY_NT_SUCCESS(_buffer->ResizeTraditional(trimmedBufferSize));

    VERIFY_IS_TRUE(_buffer->GetUnicodeStorage()._map.empty(), L"The map should now be empty.");
}

// This tests that columns removed from the buffer while resizing traditionally will also drop the high unicode
// characters from the Unicode Storage buffer
void TextBufferTests::ResizeTraditionalHighUnicodeColumnRemoval()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Get a position inside the buffer in the last column
    const COORD pos{ bufferSize.X - 1, 0 };
    auto position = _buffer->_storage[pos.Y].GetCharRow().GlyphAt(pos.X);

    // Fill it up with a sequence that will have to hit the high unicode storage.
    // This is the peach emoji: 🍑
    // It's encoded in UTF-16, as needed by the buffer.
    const auto emoji = L"\xD83C\xDF51";
    position = emoji;

    // Read back the text at that position and ensure that it matches what we wrote.
    const auto readBack = _buffer->GetTextDataAt(pos);
    const auto readBackText = *readBack;
    VERIFY_ARE_EQUAL(String(emoji), String(readBackText.data(), gsl::narrow<int>(readBackText.size())));

    VERIFY_ARE_EQUAL(1u, _buffer->GetUnicodeStorage()._map.size(), L"There should be one item in the map.");

    // Perform resize to trim off the column of the buffer that included the emoji
    COORD trimmedBufferSize{ bufferSize.X - 1, bufferSize.Y };

    VERIFY_NT_SUCCESS(_buffer->ResizeTraditional(trimmedBufferSize));

    VERIFY_IS_TRUE(_buffer->GetUnicodeStorage()._map.empty(), L"The map should now be empty.");
}

void TextBufferTests::TestBurrito()
{
    COORD bufferSize{ 80, 9001 };
    UINT cursorSize = 12;
    TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // This is the burrito emoji: 🌯
    // It's encoded in UTF-16, as needed by the buffer.
    const auto burrito = L"\xD83C\xDF2F";
    OutputCellIterator burriter{ burrito };

    auto afterFIter = _buffer->Write({ L"F" });
    _buffer->IncrementCursor();

    auto afterBurritoIter = _buffer->Write(burriter);
    _buffer->IncrementCursor();
    _buffer->IncrementCursor();
    VERIFY_IS_FALSE(afterBurritoIter);
}

void TextBufferTests::WriteLinesToBuffer(const std::vector<std::wstring>& text, TextBuffer& buffer)
{
    const auto bufferSize = buffer.GetSize();

    for (size_t row = 0; row < text.size(); ++row)
    {
        auto line = text[row];
        if (!line.empty())
        {
            // TODO GH#780: writing up to (but not past) the end of the line
            //              should NOT set the wrap flag
            std::optional<bool> wrap = true;
            if (line.size() == static_cast<size_t>(bufferSize.RightExclusive()))
            {
                wrap = std::nullopt;
            }

            OutputCellIterator iter{ line };
            buffer.Write(iter, { 0, gsl::narrow<SHORT>(row) }, wrap);
        }
    }
}

void TextBufferTests::GetWordBoundaries()
{
    COORD bufferSize{ 80, 9001 };
    UINT cursorSize = 12;
    TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Setup: Write lines of text to the buffer
    const std::vector<std::wstring> text = { L"word other",
                                             L"  more   words" };
    WriteLinesToBuffer(text, *_buffer);

    // Test Data:
    // - COORD - starting position
    // - COORD - expected result (accessibilityMode = false)
    // - COORD - expected result (accessibilityMode = true)
    struct ExpectedResult
    {
        COORD accessibilityModeDisabled;
        COORD accessibilityModeEnabled;
    };

    struct Test
    {
        COORD startPos;
        ExpectedResult expected;
    };

    // Set testData for GetWordStart tests
    // clang-format off
    std::vector<Test> testData = {
        // tests for first line of text
        { {  0, 0 },    {{  0, 0 },      { 0, 0 }} },
        { {  1, 0 },    {{  0, 0 },      { 0, 0 }} },
        { {  3, 0 },    {{  0, 0 },      { 0, 0 }} },
        { {  4, 0 },    {{  4, 0 },      { 0, 0 }} },
        { {  5, 0 },    {{  5, 0 },      { 5, 0 }} },
        { {  6, 0 },    {{  5, 0 },      { 5, 0 }} },
        { { 20, 0 },    {{ 10, 0 },      { 5, 0 }} },
        { { 79, 0 },    {{ 10, 0 },      { 5, 0 }} },

        // tests for second line of text
        { {  0, 1 },     {{ 0, 1 },       { 5, 0 }} },
        { {  1, 1 },     {{ 0, 1 },       { 5, 0 }} },
        { {  2, 1 },     {{ 2, 1 },       { 2, 1 }} },
        { {  3, 1 },     {{ 2, 1 },       { 2, 1 }} },
        { {  5, 1 },     {{ 2, 1 },       { 2, 1 }} },
        { {  6, 1 },     {{ 6, 1 },       { 2, 1 }} },
        { {  7, 1 },     {{ 6, 1 },       { 2, 1 }} },
        { {  9, 1 },     {{ 9, 1 },       { 9, 1 }} },
        { { 10, 1 },     {{ 9, 1 },       { 9, 1 }} },
        { { 20, 1 },     {{14, 1 },       { 9, 1 }} },
        { { 79, 1 },     {{14, 1 },       { 9, 1 }} },
    };
    // clang-format on

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:accessibilityMode", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool accessibilityMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"accessibilityMode", accessibilityMode), L"Get accessibility mode variant");

    const std::wstring_view delimiters = L" ";
    for (const auto& test : testData)
    {
        Log::Comment(NoThrowString().Format(L"COORD (%hd, %hd)", test.startPos.X, test.startPos.Y));
        const auto result = _buffer->GetWordStart(test.startPos, delimiters, accessibilityMode);
        const auto expected = accessibilityMode ? test.expected.accessibilityModeEnabled : test.expected.accessibilityModeDisabled;
        VERIFY_ARE_EQUAL(expected, result);
    }

    // Update testData for GetWordEnd tests
    // clang-format off
    testData = {
        // tests for first line of text
        { { 0, 0 }, { { 3, 0 }, { 5, 0 } } },
        { { 1, 0 }, { { 3, 0 }, { 5, 0 } } },
        { { 3, 0 }, { { 3, 0 }, { 5, 0 } } },
        { { 4, 0 }, { { 4, 0 }, { 5, 0 } } },
        { { 5, 0 }, { { 9, 0 }, { 2, 1 } } },
        { { 6, 0 }, { { 9, 0 }, { 2, 1 } } },
        { { 20, 0 }, { { 79, 0 }, { 2, 1 } } },
        { { 79, 0 }, { { 79, 0 }, { 2, 1 } } },

        // tests for second line of text
        { { 0, 1 }, { { 1, 1 }, { 2, 1 } } },
        { { 1, 1 }, { { 1, 1 }, { 2, 1 } } },
        { { 2, 1 }, { { 5, 1 }, { 9, 1 } } },
        { { 3, 1 }, { { 5, 1 }, { 9, 1 } } },
        { { 5, 1 }, { { 5, 1 }, { 9, 1 } } },
        { { 6, 1 }, { { 8, 1 }, { 9, 1 } } },
        { { 7, 1 }, { { 8, 1 }, { 9, 1 } } },
        { { 9, 1 }, { { 13, 1 }, { 0, 9001 } } },
        { { 10, 1 }, { { 13, 1 }, { 0, 9001 } } },
        { { 20, 1 }, { { 79, 1 }, { 0, 9001 } } },
        { { 79, 1 }, { { 79, 1 }, { 0, 9001 } } },
    };
    // clang-format on

    for (const auto& test : testData)
    {
        Log::Comment(NoThrowString().Format(L"COORD (%hd, %hd)", test.startPos.X, test.startPos.Y));
        COORD result = _buffer->GetWordEnd(test.startPos, delimiters, accessibilityMode);
        const auto expected = accessibilityMode ? test.expected.accessibilityModeEnabled : test.expected.accessibilityModeDisabled;
        VERIFY_ARE_EQUAL(expected, result);
    }
}

void TextBufferTests::MoveByWord()
{
    COORD bufferSize{ 80, 9001 };
    UINT cursorSize = 12;
    TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Setup: Write lines of text to the buffer
    const std::vector<std::wstring> text = { L"word other",
                                             L"  more   words" };
    WriteLinesToBuffer(text, *_buffer);

    // Test Data:
    // - COORD - starting position
    // - COORD - expected result (moving forwards)
    // - COORD - expected result (moving backwards)
    struct ExpectedResult
    {
        COORD moveForwards;
        COORD moveBackwards;
    };

    struct Test
    {
        COORD startPos;
        ExpectedResult expected;
    };

    // Set testData for GetWordStart tests
    // clang-format off
    std::vector<Test> testData = {
        // tests for first line of text
        { {  0, 0 },    {{  5, 0 },      { 0, 0 }} },
        { {  1, 0 },    {{  5, 0 },      { 1, 0 }} },
        { {  3, 0 },    {{  5, 0 },      { 3, 0 }} },
        { {  4, 0 },    {{  5, 0 },      { 4, 0 }} },
        { {  5, 0 },    {{  2, 1 },      { 0, 0 }} },
        { {  6, 0 },    {{  2, 1 },      { 0, 0 }} },
        { { 20, 0 },    {{  2, 1 },      { 0, 0 }} },
        { { 79, 0 },    {{  2, 1 },      { 0, 0 }} },

        // tests for second line of text
        { {  0, 1 },     {{ 2, 1 },       { 0, 0 }} },
        { {  1, 1 },     {{ 2, 1 },       { 0, 0 }} },
        { {  2, 1 },     {{ 9, 1 },       { 5, 0 }} },
        { {  3, 1 },     {{ 9, 1 },       { 5, 0 }} },
        { {  5, 1 },     {{ 9, 1 },       { 5, 0 }} },
        { {  6, 1 },     {{ 9, 1 },       { 5, 0 }} },
        { {  7, 1 },     {{ 9, 1 },       { 5, 0 }} },
        { {  9, 1 },     {{ 9, 1 },       { 2, 1 }} },
        { { 10, 1 },     {{10, 1 },       { 2, 1 }} },
        { { 20, 1 },     {{20, 1 },       { 2, 1 }} },
        { { 79, 1 },     {{79, 1 },       { 2, 1 }} },
    };
    // clang-format on

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:movingForwards", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool movingForwards;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"movingForwards", movingForwards), L"Get movingForwards variant");

    const std::wstring_view delimiters = L" ";
    const COORD lastCharPos = _buffer->GetLastNonSpaceCharacter();
    for (const auto& test : testData)
    {
        Log::Comment(NoThrowString().Format(L"COORD (%hd, %hd)", test.startPos.X, test.startPos.Y));
        auto pos{ test.startPos };
        const auto result = movingForwards ?
                                _buffer->MoveToNextWord(pos, delimiters, lastCharPos) :
                                _buffer->MoveToPreviousWord(pos, delimiters);
        const auto expected = movingForwards ? test.expected.moveForwards : test.expected.moveBackwards;
        VERIFY_ARE_EQUAL(expected, pos);

        // if we moved, result is true and pos != startPos.
        // otherwise, result is false and pos == startPos.
        VERIFY_ARE_EQUAL(result, pos != test.startPos);
    }
}

void TextBufferTests::GetGlyphBoundaries()
{
    struct ExpectedResult
    {
        std::wstring name;
        til::point start;
        til::point wideGlyphEnd;
        til::point normalEnd;
    };

    // clang-format off
    const std::vector<ExpectedResult> expected = {
        { L"Buffer Start", { 0, 0 },   { 2,  0 },   { 1,  0 } },
        { L"Line Start",   { 0, 1 },   { 2,  1 },   { 1,  1 } },
        { L"General Case", { 1, 1 },   { 3,  1 },   { 2,  1 } },
        { L"Line End",     { 9, 1 },   { 0,  2 },   { 0,  2 } },
        { L"Buffer End",   { 9, 9 },   { 0, 10 },   { 0, 10 } },
    };
    // clang-format on

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:wideGlyph", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool wideGlyph;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"wideGlyph", wideGlyph), L"Get wide glyph variant");

    COORD bufferSize{ 10, 10 };
    UINT cursorSize = 12;
    TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // This is the burrito emoji: 🌯
    // It's encoded in UTF-16, as needed by the buffer.
    const auto burrito = L"\xD83C\xDF2F";
    const wchar_t* const output = wideGlyph ? burrito : L"X";

    const OutputCellIterator iter{ output };

    for (const auto& test : expected)
    {
        Log::Comment(test.name.c_str());
        auto target = test.start;
        _buffer->Write(iter, target);

        auto start = _buffer->GetGlyphStart(target);
        auto end = _buffer->GetGlyphEnd(target);

        VERIFY_ARE_EQUAL(test.start, start);
        VERIFY_ARE_EQUAL(wideGlyph ? test.wideGlyphEnd : test.normalEnd, end);
    }
}

void TextBufferTests::GetTextRects()
{
    // GetTextRects() is used to...
    //  - Represent selection rects
    //  - Represent UiaTextRanges for accessibility

    // This is the burrito emoji: 🌯
    // It's encoded in UTF-16, as needed by the buffer.
    const auto burrito = std::wstring(L"\xD83C\xDF2F");

    COORD bufferSize{ 20, 50 };
    UINT cursorSize = 12;
    TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    // Setup: Write lines of text to the buffer
    const std::vector<std::wstring> text = { L"0123456789",
                                             L" " + burrito + L"3456" + burrito,
                                             L"  " + burrito + L"45" + burrito,
                                             burrito + L"234567" + burrito,
                                             L"0123456789" };
    WriteLinesToBuffer(text, *_buffer);
    // - - - Text Buffer Contents - - -
    // |0123456789
    // | 🌯3456🌯
    // |  🌯45🌯
    // |🌯234567🌯
    // |0123456789
    // - - - - - - - - - - - - - - - -

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:blockSelection", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool blockSelection;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blockSelection", blockSelection), L"Get 'blockSelection' variant");

    std::vector<SMALL_RECT> expected{};
    if (blockSelection)
    {
        expected.push_back({ 1, 0, 7, 0 });
        expected.push_back({ 1, 1, 8, 1 }); // expand right
        expected.push_back({ 1, 2, 7, 2 });
        expected.push_back({ 0, 3, 7, 3 }); // expand left
        expected.push_back({ 1, 4, 7, 4 });
    }
    else
    {
        expected.push_back({ 1, 0, 19, 0 });
        expected.push_back({ 0, 1, 19, 1 });
        expected.push_back({ 0, 2, 19, 2 });
        expected.push_back({ 0, 3, 19, 3 });
        expected.push_back({ 0, 4, 7, 4 });
    }

    COORD start{ 1, 0 };
    COORD end{ 7, 4 };
    const auto result = _buffer->GetTextRects(start, end, blockSelection, false);
    VERIFY_ARE_EQUAL(expected.size(), result.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        VERIFY_ARE_EQUAL(expected.at(i), result.at(i));
    }
}

void TextBufferTests::GetText()
{
    // GetText() is used by...
    //  - Copying text to the clipboard regularly
    //  - Copying text to the clipboard, with shift held (collapse to one line)
    //  - Extracting text from a UiaTextRange

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:wrappedText", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:blockSelection", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:includeCRLF", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:trimTrailingWhitespace", L"{false, true}")
    END_TEST_METHOD_PROPERTIES();

    bool wrappedText;
    bool blockSelection;
    bool includeCRLF;
    bool trimTrailingWhitespace;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"wrappedText", wrappedText), L"Get 'wrappedText' variant");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"blockSelection", blockSelection), L"Get 'blockSelection' variant");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"includeCRLF", includeCRLF), L"Get 'includeCRLF' variant");
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"trimTrailingWhitespace", trimTrailingWhitespace), L"Get 'trimTrailingWhitespace' variant");

    if (!wrappedText)
    {
        COORD bufferSize{ 10, 20 };
        UINT cursorSize = 12;
        TextAttribute attr{ 0x7f };
        auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

        // Setup: Write lines of text to the buffer
        const std::vector<std::wstring> bufferText = { L"12345",
                                                       L"  345",
                                                       L"123  ",
                                                       L"  3  " };
        WriteLinesToBuffer(bufferText, *_buffer);

        // simulate a selection from origin to {4,4}
        const auto textRects = _buffer->GetTextRects({ 0, 0 }, { 4, 4 }, blockSelection, false);

        std::wstring result = L"";
        const auto textData = _buffer->GetText(includeCRLF, trimTrailingWhitespace, textRects).text;
        for (auto& text : textData)
        {
            result += text;
        }

        std::wstring expectedText = L"";
        if (includeCRLF)
        {
            if (trimTrailingWhitespace)
            {
                Log::Comment(L"Standard Copy to Clipboard");
                expectedText += L"12345\r\n";
                expectedText += L"  345\r\n";
                expectedText += L"123\r\n";
                expectedText += L"  3\r\n";
            }
            else
            {
                Log::Comment(L"UI Automation");
                if (blockSelection)
                {
                    expectedText += L"12345\r\n";
                    expectedText += L"  345\r\n";
                    expectedText += L"123  \r\n";
                    expectedText += L"  3  \r\n";
                    expectedText += L"     ";
                }
                else
                {
                    expectedText += L"12345     \r\n";
                    expectedText += L"  345     \r\n";
                    expectedText += L"123       \r\n";
                    expectedText += L"  3       \r\n";
                    expectedText += L"     ";
                }
            }
        }
        else
        {
            if (trimTrailingWhitespace)
            {
                Log::Comment(L"UNDEFINED");
                expectedText += L"12345";
                expectedText += L"  345";
                expectedText += L"123";
                expectedText += L"  3";
            }
            else
            {
                Log::Comment(L"Shift+Copy to Clipboard");
                if (blockSelection)
                {
                    expectedText += L"12345";
                    expectedText += L"  345";
                    expectedText += L"123  ";
                    expectedText += L"  3  ";
                    expectedText += L"     ";
                }
                else
                {
                    expectedText += L"12345     ";
                    expectedText += L"  345     ";
                    expectedText += L"123       ";
                    expectedText += L"  3       ";
                    expectedText += L"     ";
                }
            }
        }

        // Verify expected output and actual output are the same
        VERIFY_ARE_EQUAL(expectedText, result);
    }
    else
    {
        // Case 2: Wrapped Text
        COORD bufferSize{ 5, 20 };
        UINT cursorSize = 12;
        TextAttribute attr{ 0x7f };
        auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

        // Setup: Write lines of text to the buffer
        const std::vector<std::wstring> bufferText = { L"1234567",
                                                       L"",
                                                       L"  345",
                                                       L"123    ",
                                                       L"" };
        WriteLinesToBuffer(bufferText, *_buffer);
        // buffer should look like this:
        // ______
        // |12345| <-- wrapped
        // |67   |
        // |  345|
        // |123  | <-- wrapped
        // |     |
        // |_____|

        // simulate a selection from origin to {4,5}
        const auto textRects = _buffer->GetTextRects({ 0, 0 }, { 4, 5 }, blockSelection, false);

        std::wstring result = L"";

        const auto formatWrappedRows = blockSelection;
        const auto textData = _buffer->GetText(includeCRLF, trimTrailingWhitespace, textRects, nullptr, formatWrappedRows).text;
        for (auto& text : textData)
        {
            result += text;
        }

        std::wstring expectedText = L"";
        if (formatWrappedRows)
        {
            if (includeCRLF)
            {
                if (trimTrailingWhitespace)
                {
                    Log::Comment(L"UNDEFINED");
                    expectedText += L"12345\r\n";
                    expectedText += L"67\r\n";
                    expectedText += L"  345\r\n";
                    expectedText += L"123\r\n";
                    expectedText += L"\r\n";
                }
                else
                {
                    Log::Comment(L"Copy block selection to Clipboard");
                    expectedText += L"12345\r\n";
                    expectedText += L"67   \r\n";
                    expectedText += L"  345\r\n";
                    expectedText += L"123  \r\n";
                    expectedText += L"     \r\n";
                    expectedText += L"     ";
                }
            }
            else
            {
                if (trimTrailingWhitespace)
                {
                    Log::Comment(L"UNDEFINED");
                    expectedText += L"12345";
                    expectedText += L"67";
                    expectedText += L"  345";
                    expectedText += L"123";
                }
                else
                {
                    Log::Comment(L"UNDEFINED");
                    expectedText += L"12345";
                    expectedText += L"67   ";
                    expectedText += L"  345";
                    expectedText += L"123  ";
                    expectedText += L"     ";
                    expectedText += L"     ";
                }
            }
        }
        else
        {
            if (includeCRLF)
            {
                if (trimTrailingWhitespace)
                {
                    Log::Comment(L"Standard Copy to Clipboard");
                    expectedText += L"12345";
                    expectedText += L"67\r\n";
                    expectedText += L"  345\r\n";
                    expectedText += L"123  \r\n";
                }
                else
                {
                    Log::Comment(L"UI Automation");
                    expectedText += L"12345";
                    expectedText += L"67   \r\n";
                    expectedText += L"  345\r\n";
                    expectedText += L"123  ";
                    expectedText += L"     \r\n";
                    expectedText += L"     ";
                }
            }
            else
            {
                if (trimTrailingWhitespace)
                {
                    Log::Comment(L"UNDEFINED");
                    expectedText += L"12345";
                    expectedText += L"67";
                    expectedText += L"  345";
                    expectedText += L"123  ";
                }
                else
                {
                    Log::Comment(L"Shift+Copy to Clipboard");
                    expectedText += L"12345";
                    expectedText += L"67   ";
                    expectedText += L"  345";
                    expectedText += L"123  ";
                    expectedText += L"     ";
                    expectedText += L"     ";
                }
            }
        }

        // Verify expected output and actual output are the same
        VERIFY_ARE_EQUAL(expectedText, result);
    }
}

// This tests that when we increment the circular buffer, obsolete hyperlink references
// are removed from the hyperlink map
void TextBufferTests::HyperlinkTrim()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    const auto url = L"test.url";
    const auto otherUrl = L"other.url";
    const auto customId = L"CustomId";
    const auto otherCustomId = L"OtherCustomId";

    // Set a hyperlink id in the first row and add a hyperlink to our map
    const COORD pos{ 70, 0 };
    const auto id = _buffer->GetHyperlinkId(url, customId);
    TextAttribute newAttr{ 0x7f };
    newAttr.SetHyperlinkId(id);
    _buffer->GetRowByOffset(pos.Y).GetAttrRow().SetAttrToEnd(pos.X, newAttr);
    _buffer->AddHyperlinkToMap(url, id);

    // Set a different hyperlink id somewhere else in the buffer
    const COORD otherPos{ 70, 5 };
    const auto otherId = _buffer->GetHyperlinkId(otherUrl, otherCustomId);
    newAttr.SetHyperlinkId(otherId);
    _buffer->GetRowByOffset(otherPos.Y).GetAttrRow().SetAttrToEnd(otherPos.X, newAttr);
    _buffer->AddHyperlinkToMap(otherUrl, otherId);

    // Increment the circular buffer
    _buffer->IncrementCircularBuffer();

    const auto finalCustomId = fmt::format(L"{}%{}", customId, std::hash<std::wstring_view>{}(url));
    const auto finalOtherCustomId = fmt::format(L"{}%{}", otherCustomId, std::hash<std::wstring_view>{}(otherUrl));

    // The hyperlink reference that was only in the first row should be deleted from the map
    VERIFY_ARE_EQUAL(_buffer->_hyperlinkMap.find(id), _buffer->_hyperlinkMap.end());
    // Since there was a custom id, that should be deleted as well
    VERIFY_ARE_EQUAL(_buffer->_hyperlinkCustomIdMap.find(finalCustomId), _buffer->_hyperlinkCustomIdMap.end());

    // The other hyperlink reference should not be deleted
    VERIFY_ARE_EQUAL(_buffer->_hyperlinkMap[otherId], otherUrl);
    VERIFY_ARE_EQUAL(_buffer->_hyperlinkCustomIdMap[finalOtherCustomId], otherId);
}

// This tests that when we increment the circular buffer, non-obsolete hyperlink references
// do not get removed from the hyperlink map
void TextBufferTests::NoHyperlinkTrim()
{
    // Set up a text buffer for us
    const COORD bufferSize{ 80, 10 };
    const UINT cursorSize = 12;
    const TextAttribute attr{ 0x7f };
    auto _buffer = std::make_unique<TextBuffer>(bufferSize, attr, cursorSize, _renderTarget);

    const auto url = L"test.url";
    const auto customId = L"CustomId";

    // Set a hyperlink id in the first row and add a hyperlink to our map
    const COORD pos{ 70, 0 };
    const auto id = _buffer->GetHyperlinkId(url, customId);
    TextAttribute newAttr{ 0x7f };
    newAttr.SetHyperlinkId(id);
    _buffer->GetRowByOffset(pos.Y).GetAttrRow().SetAttrToEnd(pos.X, newAttr);
    _buffer->AddHyperlinkToMap(url, id);

    // Set the same hyperlink id somewhere else in the buffer
    const COORD otherPos{ 70, 5 };
    _buffer->GetRowByOffset(otherPos.Y).GetAttrRow().SetAttrToEnd(otherPos.X, newAttr);

    // Increment the circular buffer
    _buffer->IncrementCircularBuffer();

    const auto finalCustomId = fmt::format(L"{}%{}", customId, std::hash<std::wstring_view>{}(url));

    // The hyperlink reference should not be deleted from the map since it is still present in the buffer
    VERIFY_ARE_EQUAL(_buffer->GetHyperlinkUriFromId(id), url);
    VERIFY_ARE_EQUAL(_buffer->_hyperlinkCustomIdMap[finalCustomId], id);
}
