/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* This File was generated using the VisualTAEF C++ Project Wizard.
* Class Name: SelectionTest
*/
#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../cascadia/UnitTests_TerminalCore/MockTermSettings.h"
#include "../renderer/inc/DummyRenderer.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace winrt::Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
    class SelectionTest
    {
        TEST_CLASS(SelectionTest);

        // Method Description:
        // - Validate a selection that spans only one row
        // Arguments:
        // - term: the terminal that is contains the selection
        // - expected: the expected value of the selection rect
        // Return Value:
        // - N/A
        void ValidateLinearSelection(Terminal& term, const til::point start, const til::point end)
        {
            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionSpans = term.GetSelectionSpans();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionSpans.size(), static_cast<size_t>(1));

            auto& sp{ selectionSpans[0] };
            VERIFY_ARE_EQUAL(start, sp.start, L"start");
            VERIFY_ARE_EQUAL(end, sp.end, L"end");
        }

        TextBuffer& GetTextBuffer(Terminal& term)
        {
            return term.GetBufferAndViewport().buffer;
        }

        TEST_METHOD(SelectUnit)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            ValidateLinearSelection(term, { 5, 10 }, { 5, 10 });
        }

        TEST_METHOD(SelectArea)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            til::CoordType rowValue = 10;

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetSelectionEnd({ 15, 20 });

            // Validate selection area
            ValidateLinearSelection(term, { 5, rowValue }, { 15, 20 });
        }

        TEST_METHOD(OverflowTests)
        {
            const til::point maxCoord = { SHRT_MAX, SHRT_MAX };

            // Test SetSelectionAnchor(til::point) and SetSelectionEnd(til::point)
            // Behavior: clamp coord to viewport.
            auto ValidateSingleClickSelection = [&](til::CoordType scrollback, const til::point start, const til::point end) {
                Terminal term{ Terminal::TestDummyMarker{} };
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                // NOTE: SetSelectionEnd(til::point) is called within SetSelectionAnchor(til::point)
                term.SetSelectionAnchor(maxCoord);
                ValidateLinearSelection(term, start, end);
            };

            // Test a Double Click Selection
            // Behavior: clamp coord to viewport.
            //           Then, do double click selection.
            auto ValidateDoubleClickSelection = [&](til::CoordType scrollback, const til::point start, const til::point end) {
                Terminal term{ Terminal::TestDummyMarker{} };
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                term.MultiClickSelection(maxCoord, Terminal::SelectionExpansion::Word);
                ValidateLinearSelection(term, start, end);
            };

            // Test a Triple Click Selection
            // Behavior: clamp coord to viewport.
            //           Then, do triple click selection.
            auto ValidateTripleClickSelection = [&](til::CoordType scrollback, const til::point start, const til::point end) {
                Terminal term{ Terminal::TestDummyMarker{} };
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                term.MultiClickSelection(maxCoord, Terminal::SelectionExpansion::Line);
                ValidateLinearSelection(term, start, end);
            };

            // Test with no scrollback
            Log::Comment(L"Single click selection with NO scrollback value");
            ValidateSingleClickSelection(0, { 10, 9 }, { 10, 9 });
            Log::Comment(L"Double click selection with NO scrollback value");
            ValidateDoubleClickSelection(0, { 0, 9 }, { 10, 9 });
            Log::Comment(L"Triple click selection with NO scrollback value");
            ValidateTripleClickSelection(0, { 0, 9 }, { 10, 9 });

            // Test with max scrollback
            const til::CoordType expected_row = SHRT_MAX - 1;
            Log::Comment(L"Single click selection with MAXIMUM scrollback value");
            ValidateSingleClickSelection(SHRT_MAX, { 10, expected_row }, { 10, expected_row });
            Log::Comment(L"Double click selection with MAXIMUM scrollback value");
            ValidateDoubleClickSelection(SHRT_MAX, { 0, expected_row }, { 10, expected_row });
            Log::Comment(L"Triple click selection with MAXIMUM scrollback value");
            ValidateTripleClickSelection(SHRT_MAX, { 0, expected_row }, { 10, expected_row });
        }

        TEST_METHOD(SelectFromOutofBounds)
        {
            /*  NOTE:
                ensuring that the selection anchors are clamped to be valid permits us to make the following assumption:
                    - All selection expansion functions will operate as if they were performed at the boundary
            */

            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 10, 10 }, 0, renderer);

            auto viewport = term.GetViewport();
            const auto leftBoundary = viewport.Left();
            const auto rightExclusiveBoundary = viewport.RightExclusive();
            const auto topBoundary = viewport.Top();
            const auto bottomBoundary = viewport.BottomInclusive();

            // Case 1: Simulate click past right (x,y) = (20,5)
            // should clamp to right boundary
            term.SetSelectionAnchor({ 20, 5 });
            Log::Comment(L"Out of bounds: X-value too large");
            ValidateLinearSelection(term, { rightExclusiveBoundary, 5 }, { rightExclusiveBoundary, 5 });

            // Case 2: Simulate click past left (x,y) = (-20,5)
            // should clamp to left boundary
            term.SetSelectionAnchor({ -20, 5 });
            Log::Comment(L"Out of bounds: X-value too negative");
            ValidateLinearSelection(term, { leftBoundary, 5 }, { leftBoundary, 5 });

            // Case 3: Simulate click past top (x,y) = (5,-20)
            // should clamp to top boundary
            term.SetSelectionAnchor({ 5, -20 });
            Log::Comment(L"Out of bounds: Y-value too negative");
            ValidateLinearSelection(term, { 5, topBoundary }, { 5, topBoundary });

            // Case 4: Simulate click past bottom (x,y) = (5,20)
            // should clamp to bottom boundary
            term.SetSelectionAnchor({ 5, 20 });
            Log::Comment(L"Out of bounds: Y-value too large");
            ValidateLinearSelection(term, { 5, bottomBoundary }, { 5, bottomBoundary });
        }

        TEST_METHOD(SelectToOutOfBounds)
        {
            /*  NOTE:
                ensuring that the selection anchors are clamped to be valid permits us to make the following assumption:
                    - All selection expansion functions will operate as if they were performed at the boundary
            */

            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 10, 10 }, 0, renderer);

            auto viewport = term.GetViewport();
            const til::CoordType leftBoundary = 0;
            const auto rightExclusiveBoundary = viewport.RightExclusive();

            // Simulate click at (x,y) = (5,5)
            term.SetSelectionAnchor({ 5, 5 });

            // Case 1: Move out of right boundary
            Log::Comment(L"Out of bounds: X-value too large");
            term.SetSelectionEnd({ 20, 5 });
            ValidateLinearSelection(term, { 5, 5 }, { rightExclusiveBoundary, 5 });

            // Case 2: Move out of left boundary
            Log::Comment(L"Out of bounds: X-value negative");
            term.SetSelectionEnd({ -20, 5 });
            ValidateLinearSelection(term, { leftBoundary, 5 }, { 5, 5 });

            // Case 3: Move out of top boundary
            Log::Comment(L"Out of bounds: Y-value negative");
            term.SetSelectionEnd({ 5, -20 });
            ValidateLinearSelection(term, { 5, 0 }, { 5, 5 });

            // Case 4: Move out of bottom boundary
            Log::Comment(L"Out of bounds: Y-value too large");
            term.SetSelectionEnd({ 5, 20 });
            ValidateLinearSelection(term, { 5, 5 }, { 5, 9 });
        }

        TEST_METHOD(SelectBoxArea)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            til::CoordType rowValue = 10;

            // Simulate ALT + click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });
            term.SetBlockSelection(true);

            // Simulate move to (x,y) = (15,20)
            term.SetSelectionEnd({ 15, 20 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionSpans = term.GetSelectionSpans();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionSpans.size(), static_cast<size_t>(11));

            for (auto&& sp : selectionSpans)
            {
                VERIFY_ARE_EQUAL((til::point{ 5, rowValue }), sp.start);
                VERIFY_ARE_EQUAL((til::point{ 15, rowValue }), sp.end);
                rowValue++;
            }
        }

        TEST_METHOD(SelectAreaAfterScroll)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            til::CoordType scrollbackLines = 100;
            term.Create({ 120, 30 }, scrollbackLines, renderer);

            const til::CoordType contentScrollLines = 15;
            // Simulate a content-initiated scroll down by 15 lines
            term.SetViewportPosition({ 0, contentScrollLines });

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            til::CoordType rowValue = 10;

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetSelectionEnd({ 15, 20 });

            ValidateLinearSelection(term, { 5, contentScrollLines + rowValue }, { 15, contentScrollLines + 20 });

            const til::CoordType userScrollViewportTop = 10;
            // Simulate a user-initiated scroll *up* to line 10 (NOTE: Not *up by 10 lines*)
            term.UserScrollViewport(userScrollViewportTop);

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetSelectionEnd({ 15, 20 });

            ValidateLinearSelection(term, { 5, userScrollViewportTop + rowValue }, { 15, userScrollViewportTop + 20 });
        }

        TEST_METHOD(SelectWideGlyph_Trailing)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            // Validate selection area
            // Selection should expand one to the left to get the leading half of the wide glyph
            ValidateLinearSelection(term, { 4, 10 }, { 6, 10 });
        }

        TEST_METHOD(SelectWideGlyph_Leading)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 4, 10 };
            term.SetSelectionAnchor(clickPos);

            // Validate selection area
            // Selection should clamp to the left side of the glyph and stay degenerate
            ValidateLinearSelection(term, { 4, 10 }, { 4, 10 });
        }

        TEST_METHOD(SelectWideGlyphsInBoxSelection)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Insert wide glyph at position (7,11)
            GetTextBuffer(term).GetCursor().SetPosition({ 7, 11 });
            term.Write(burrito);

            // Text buffer should look like this:
            // -------------
            // |     A      |
            // |            |
            // |    🌯      |
            // |       🌯   |
            // |        B   |
            // -------------
            // A: selection anchor
            // B: selection end
            // The boundaries of the selection should cut through
            //  the middle of the burritos, but the selection
            //  should expand to encompass each burrito entirely.

            // Simulate ALT + click at (x,y) = (5,8)
            term.SetSelectionAnchor({ 5, 8 });
            term.SetBlockSelection(true);

            // Simulate move to (x,y) = (8,12)
            term.SetSelectionEnd({ 8, 12 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionSpans = term.GetSelectionSpans();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionSpans.size(), static_cast<size_t>(5));

            auto viewport = term.GetViewport();
            til::CoordType rowValue = 8;
            for (auto&& sp : selectionSpans)
            {
                if (rowValue == 10)
                {
                    VERIFY_ARE_EQUAL((til::point{ 4, rowValue }), sp.start);
                    VERIFY_ARE_EQUAL((til::point{ 8, rowValue }), sp.end);
                }
                else if (rowValue == 11)
                {
                    VERIFY_ARE_EQUAL((til::point{ 5, rowValue }), sp.start);
                    VERIFY_ARE_EQUAL((til::point{ 9, rowValue }), sp.end);
                }
                else
                {
                    // Verify all lines
                    VERIFY_ARE_EQUAL((til::point{ 5, rowValue }), sp.start);
                    VERIFY_ARE_EQUAL((til::point{ 8, rowValue }), sp.end);
                }

                rowValue++;
            }
        }

        TEST_METHOD(DoubleClick_GeneralCase)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe";
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate double click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // Validate selection area
            ValidateLinearSelection(term, { 4, 10 }, { gsl::narrow<til::CoordType>(4 + text.size()), 10 });
        }

        TEST_METHOD(DoubleClick_Delimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // Validate selection area
            ValidateLinearSelection(term, { 0, 10 }, { term.GetViewport().RightExclusive(), 10 });
        }

        TEST_METHOD(DoubleClick_DelimiterClass)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate click at (x,y) = (15,10)
            // this is over the '>' char
            auto clickPos = til::point{ 15, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // ---Validate selection area---
            // "Terminal" is in class 2
            // ":" and ">" are in class 1
            // the white space to the right of the ">" is in class 0
            // Double-clicking the ">" should only highlight that cell
            ValidateLinearSelection(term, { 15, 10 }, { 16, 10 });
        }

        TEST_METHOD(DoubleClickDrag_Right)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate double click at (x,y) = (5,10)
            term.MultiClickSelection({ 5, 10 }, Terminal::SelectionExpansion::Word);

            // Simulate move to (x,y) = (21,10)
            //
            // buffer: doubleClickMe dragThroughHere
            //         ^                ^
            //       start            finish
            term.SetSelectionEnd({ 21, 10 });

            // Validate selection area
            ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
        }

        TEST_METHOD(DoubleClickDrag_Left)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate double click at (x,y) = (21,10)
            term.MultiClickSelection({ 21, 10 }, Terminal::SelectionExpansion::Word);

            // Simulate move to (x,y) = (5,10)
            //
            // buffer: doubleClickMe dragThroughHere
            //          ^               ^
            //        finish           start
            term.SetSelectionEnd({ 5, 10 });

            // Validate selection area
            ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
        }

        TEST_METHOD(TripleClick_GeneralCase)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Validate selection area
            ValidateLinearSelection(term, { 0, 10 }, { term.GetViewport().RightExclusive(), 10 });
        }

        TEST_METHOD(TripleClick_WrappedLine)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 10, 5 }, 0, renderer);
            term.Write(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ");

            // Simulate click at (x,y) = (3,1)
            auto clickPos = til::point{ 3, 1 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Validate selection area
            ValidateLinearSelection(term, { 0, 0 }, { term.GetViewport().RightExclusive(), 2 });
        }

        TEST_METHOD(TripleClickDrag_Horizontal)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Simulate move to (x,y) = (7,10)
            term.SetSelectionEnd({ 7, 10 });

            // Validate selection area
            ValidateLinearSelection(term, { 0, 10 }, { term.GetViewport().RightExclusive(), 10 });
        }

        TEST_METHOD(TripleClickDrag_Vertical)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Simulate move to (x,y) = (5,11)
            term.SetSelectionEnd({ 5, 11 });

            ValidateLinearSelection(term, { 0, 10 }, { term.GetViewport().RightExclusive(), 11 });
        }

        TEST_METHOD(ShiftClick)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere anotherWord";
            GetTextBuffer(term).GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            Log::Comment(L"Step 1 : Create a selection on \"doubleClickMe\"");
            {
                // Simulate double click at (x,y) = (5,10)
                term.MultiClickSelection({ 5, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe" selected
                ValidateLinearSelection(term, { 4, 10 }, { 17, 10 });
            }

            Log::Comment(L"Step 2: Shift+Click to \"dragThroughHere\"");
            {
                // Simulate Shift+Click at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^
                //       start            finish
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area: "doubleClickMe drag" selected
                ValidateLinearSelection(term, { 4, 10 }, { 22, 10 });
            }

            Log::Comment(L"Step 3: Shift+Double-Click at \"dragThroughHere\"");
            {
                // Simulate Shift+DoubleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^          ^
                //       start            click      finish
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
            }

            Log::Comment(L"Step 4: Shift+Triple-Click at \"dragThroughHere\"");
            {
                // Simulate Shift+TripleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere     |
                //         ^                ^                ^
                //       start            click            finish (boundary)
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Line);

                // Validate selection area: "doubleClickMe dragThroughHere..." selected
                ValidateLinearSelection(term, { 4, 10 }, { 100, 10 });
            }

            Log::Comment(L"Step 5: Shift+Double-Click at \"dragThroughHere\"");
            {
                // Simulate Shift+DoubleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere anotherWord
                //         ^                ^           ^
                //       start            click       finish
                // NOTE: end is exclusive, so finish should point to the spot AFTER "dragThroughHere"
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
            }

            Log::Comment(L"Step 6: Drag past \"dragThroughHere\"");
            {
                // Simulate drag to (x,y) = (35,10)
                // Since we were preceded by a double-click, we're in "word" expansion mode
                //
                // buffer: doubleClickMe dragThroughHere anotherWord
                //         ^                              ^
                //       start                          finish
                term.SetSelectionEnd({ 35, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere anotherWord" selected
                ValidateLinearSelection(term, { 4, 10 }, { 45, 10 });
            }

            Log::Comment(L"Step 7: Drag back to \"dragThroughHere\"");
            {
                // Simulate drag to (x,y) = (21,10)
                // Should still be in "word" expansion mode!
                //
                // buffer: doubleClickMe dragThroughHere anotherWord
                //         ^                ^
                //       start            finish
                term.SetSelectionEnd({ 21, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
            }

            Log::Comment(L"Step 8: Drag within \"dragThroughHere\"");
            {
                // Simulate drag to (x,y) = (25,10)
                //
                // buffer: doubleClickMe dragThroughHere anotherWord
                //         ^                    ^
                //       start                finish
                term.SetSelectionEnd({ 25, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere" still selected
                ValidateLinearSelection(term, { 4, 10 }, { 33, 10 });
            }
        }

        TEST_METHOD(Pivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            Log::Comment(L"Step 1: Create a selection");
            {
                // (10,10) to (20, 10) (inclusive)
                term.SelectNewRegion({ 10, 10 }, { 20, 10 });

                // Validate selection area
                ValidateLinearSelection(term, { 10, 10 }, { 21, 10 });
            }

            Log::Comment(L"Step 2: Drag to (5,10)");
            {
                term.SetSelectionEnd({ 5, 10 });

                // Validate selection area
                // NOTES:
                // - Pivot should be (10, 10)
                // - though end is generally exclusive, since we moved behind the pivot, end is actually inclusive
                ValidateLinearSelection(term, { 5, 10 }, { 10, 10 });
            }

            Log::Comment(L"Step 3: Drag back to (20,10)");
            {
                term.SetSelectionEnd({ 20, 10 });

                // Validate selection area
                // NOTE: Pivot should still be (10, 10)
                ValidateLinearSelection(term, { 10, 10 }, { 20, 10 });
            }

            Log::Comment(L"Step 4: Shift+Click at (5,10)");
            {
                term.SetSelectionEnd({ 5, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area
                // NOTE: Pivot should still be (10, 10)
                ValidateLinearSelection(term, { 5, 10 }, { 10, 10 });
            }

            Log::Comment(L"Step 5: Shift+Click back at (20,10)");
            {
                term.SetSelectionEnd({ 20, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area
                //   Pivot should still be (10, 10)
                //   Shift+Click makes end inclusive (so add 1)
                ValidateLinearSelection(term, { 10, 10 }, { 21, 10 });
            }
        }
    };
}
