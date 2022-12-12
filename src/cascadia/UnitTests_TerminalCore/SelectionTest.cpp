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
        void ValidateSingleRowSelection(Terminal& term, const til::inclusive_rect& expected)
        {
            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));
            auto selection = term.GetViewport().ConvertToOrigin(selectionRects[0]).ToInclusive();

            VERIFY_ARE_EQUAL(selection, expected);
        }

        TEST_METHOD(SelectUnit)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            ValidateSingleRowSelection(term, { 5, 10, 5, 10 });
        }

        TEST_METHOD(SelectArea)
        {
            Terminal term;
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

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            auto rightBoundary = viewport.RightInclusive();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    // Verify top line
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, 10, rightBoundary, 10 }));
                }
                else if (rowValue == 20)
                {
                    // Verify bottom line
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, 20, 15, 20 }));
                }
                else
                {
                    // Verify other lines (full)
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, rowValue, rightBoundary, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(OverflowTests)
        {
            const til::point maxCoord = { SHRT_MAX, SHRT_MAX };

            // Test SetSelectionAnchor(til::point) and SetSelectionEnd(til::point)
            // Behavior: clamp coord to viewport.
            auto ValidateSingleClickSelection = [&](til::CoordType scrollback, const til::inclusive_rect& expected) {
                Terminal term;
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                // NOTE: SetSelectionEnd(til::point) is called within SetSelectionAnchor(til::point)
                term.SetSelectionAnchor(maxCoord);
                ValidateSingleRowSelection(term, expected);
            };

            // Test a Double Click Selection
            // Behavior: clamp coord to viewport.
            //           Then, do double click selection.
            auto ValidateDoubleClickSelection = [&](til::CoordType scrollback, const til::inclusive_rect& expected) {
                Terminal term;
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                term.MultiClickSelection(maxCoord, Terminal::SelectionExpansion::Word);
                ValidateSingleRowSelection(term, expected);
            };

            // Test a Triple Click Selection
            // Behavior: clamp coord to viewport.
            //           Then, do triple click selection.
            auto ValidateTripleClickSelection = [&](til::CoordType scrollback, const til::inclusive_rect& expected) {
                Terminal term;
                DummyRenderer renderer{ &term };
                term.Create({ 10, 10 }, scrollback, renderer);

                term.MultiClickSelection(maxCoord, Terminal::SelectionExpansion::Line);
                ValidateSingleRowSelection(term, expected);
            };

            // Test with no scrollback
            Log::Comment(L"Single click selection with NO scrollback value");
            ValidateSingleClickSelection(0, { 9, 9, 9, 9 });
            Log::Comment(L"Double click selection with NO scrollback value");
            ValidateDoubleClickSelection(0, { 0, 9, 9, 9 });
            Log::Comment(L"Triple click selection with NO scrollback value");
            ValidateTripleClickSelection(0, { 0, 9, 9, 9 });

            // Test with max scrollback
            const til::CoordType expected_row = SHRT_MAX - 1;
            Log::Comment(L"Single click selection with MAXIMUM scrollback value");
            ValidateSingleClickSelection(SHRT_MAX, { 9, expected_row, 9, expected_row });
            Log::Comment(L"Double click selection with MAXIMUM scrollback value");
            ValidateDoubleClickSelection(SHRT_MAX, { 0, expected_row, 9, expected_row });
            Log::Comment(L"Triple click selection with MAXIMUM scrollback value");
            ValidateTripleClickSelection(SHRT_MAX, { 0, expected_row, 9, expected_row });
        }

        TEST_METHOD(SelectFromOutofBounds)
        {
            /*  NOTE:
                ensuring that the selection anchors are clamped to be valid permits us to make the following assumption:
                    - All selection expansion functions will operate as if they were performed at the boundary
            */

            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 10, 10 }, 0, renderer);

            auto viewport = term.GetViewport();
            const auto leftBoundary = viewport.Left();
            const auto rightBoundary = viewport.RightInclusive();
            const auto topBoundary = viewport.Top();
            const auto bottomBoundary = viewport.BottomInclusive();

            // Case 1: Simulate click past right (x,y) = (20,5)
            // should clamp to right boundary
            term.SetSelectionAnchor({ 20, 5 });
            Log::Comment(L"Out of bounds: X-value too large");
            ValidateSingleRowSelection(term, { rightBoundary, 5, rightBoundary, 5 });

            // Case 2: Simulate click past left (x,y) = (-20,5)
            // should clamp to left boundary
            term.SetSelectionAnchor({ -20, 5 });
            Log::Comment(L"Out of bounds: X-value too negative");
            ValidateSingleRowSelection(term, { leftBoundary, 5, leftBoundary, 5 });

            // Case 3: Simulate click past top (x,y) = (5,-20)
            // should clamp to top boundary
            term.SetSelectionAnchor({ 5, -20 });
            Log::Comment(L"Out of bounds: Y-value too negative");
            ValidateSingleRowSelection(term, { 5, topBoundary, 5, topBoundary });

            // Case 4: Simulate click past bottom (x,y) = (5,20)
            // should clamp to bottom boundary
            term.SetSelectionAnchor({ 5, 20 });
            Log::Comment(L"Out of bounds: Y-value too large");
            ValidateSingleRowSelection(term, { 5, bottomBoundary, 5, bottomBoundary });
        }

        TEST_METHOD(SelectToOutOfBounds)
        {
            /*  NOTE:
                ensuring that the selection anchors are clamped to be valid permits us to make the following assumption:
                    - All selection expansion functions will operate as if they were performed at the boundary
            */

            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 10, 10 }, 0, renderer);

            auto viewport = term.GetViewport();
            const til::CoordType leftBoundary = 0;
            const auto rightBoundary = viewport.RightInclusive();

            // Simulate click at (x,y) = (5,5)
            term.SetSelectionAnchor({ 5, 5 });

            // Case 1: Move out of right boundary
            Log::Comment(L"Out of bounds: X-value too large");
            term.SetSelectionEnd({ 20, 5 });
            ValidateSingleRowSelection(term, til::inclusive_rect({ 5, 5, rightBoundary, 5 }));

            // Case 2: Move out of left boundary
            Log::Comment(L"Out of bounds: X-value negative");
            term.SetSelectionEnd({ -20, 5 });
            ValidateSingleRowSelection(term, { leftBoundary, 5, 5, 5 });

            // Case 3: Move out of top boundary
            Log::Comment(L"Out of bounds: Y-value negative");
            term.SetSelectionEnd({ 5, -20 });
            {
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(6));
                for (auto selectionRect : selectionRects)
                {
                    auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();
                    auto rowValue = selectionRect.BottomInclusive();

                    if (rowValue == 0)
                    {
                        // Verify top line
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, rowValue, rightBoundary, rowValue }));
                    }
                    else if (rowValue == 5)
                    {
                        // Verify last line
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ leftBoundary, rowValue, 5, rowValue }));
                    }
                    else
                    {
                        // Verify other lines (full)
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ leftBoundary, rowValue, rightBoundary, rowValue }));
                    }
                }
            }

            // Case 4: Move out of bottom boundary
            Log::Comment(L"Out of bounds: Y-value too large");
            term.SetSelectionEnd({ 5, 20 });
            {
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(5));
                for (auto selectionRect : selectionRects)
                {
                    auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();
                    auto rowValue = selectionRect.BottomInclusive();

                    if (rowValue == 5)
                    {
                        // Verify top line
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, 5, rightBoundary, 5 }));
                    }
                    else if (rowValue == 9)
                    {
                        // Verify bottom line
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ leftBoundary, rowValue, 5, rowValue }));
                    }
                    else
                    {
                        // Verify other lines (full)
                        VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ leftBoundary, rowValue, rightBoundary, rowValue }));
                    }
                }
            }
        }

        TEST_METHOD(SelectBoxArea)
        {
            Terminal term;
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
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                // Verify all lines
                VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, rowValue, 15, rowValue }));

                rowValue++;
            }
        }

        TEST_METHOD(SelectAreaAfterScroll)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            til::CoordType scrollbackLines = 5;
            term.Create({ 100, 100 }, scrollbackLines, renderer);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            til::CoordType rowValue = 10;

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetSelectionEnd({ 15, 20 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            auto rightBoundary = viewport.RightInclusive();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    // Verify top line
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, 10, rightBoundary, 10 }));
                }
                else if (rowValue == 20)
                {
                    // Verify bottom line
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, 20, 15, 20 }));
                }
                else
                {
                    // Verify other lines (full)
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, rowValue, rightBoundary, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(SelectWideGlyph_Trailing)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            // Validate selection area
            // Selection should expand one to the left to get the leading half of the wide glyph
            ValidateSingleRowSelection(term, { 4, 10, 5, 10 });
        }

        TEST_METHOD(SelectWideGlyph_Leading)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 4, 10 };
            term.SetSelectionAnchor(clickPos);

            // Validate selection area
            // Selection should expand one to the left to get the leading half of the wide glyph
            ValidateSingleRowSelection(term, { 4, 10, 5, 10 });
        }

        TEST_METHOD(SelectWideGlyphsInBoxSelection)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(burrito);

            // Insert wide glyph at position (7,11)
            term.GetTextBuffer().GetCursor().SetPosition({ 7, 11 });
            term.Write(burrito);

            // Simulate ALT + click at (x,y) = (5,8)
            term.SetSelectionAnchor({ 5, 8 });
            term.SetBlockSelection(true);

            // Simulate move to (x,y) = (7,12)
            term.SetSelectionEnd({ 7, 12 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(5));

            auto viewport = term.GetViewport();
            til::CoordType rowValue = 8;
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 4, rowValue, 7, rowValue }));
                }
                else if (rowValue == 11)
                {
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, rowValue, 8, rowValue }));
                }
                else
                {
                    // Verify all lines
                    VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 5, rowValue, 7, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(DoubleClick_GeneralCase)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe";
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate double click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // Validate selection area
            ValidateSingleRowSelection(term, til::inclusive_rect{ 4, 10, gsl::narrow<til::CoordType>(4 + text.size() - 1), 10 });
        }

        TEST_METHOD(DoubleClick_Delimiter)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            ValidateSingleRowSelection(term, til::inclusive_rect({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(DoubleClick_DelimiterClass)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"C:\\Terminal>";
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate click at (x,y) = (15,10)
            // this is over the '>' char
            auto clickPos = til::point{ 15, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Word);

            // ---Validate selection area---
            // "Terminal" is in class 2
            // ">" is in class 1
            // the white space to the right of the ">" is in class 0
            // Double-clicking the ">" should only highlight that cell
            ValidateSingleRowSelection(term, til::inclusive_rect({ 15, 10, 15, 10 }));
        }

        TEST_METHOD(DoubleClickDrag_Right)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
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
            ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
        }

        TEST_METHOD(DoubleClickDrag_Left)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (21,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Simulate double click at (x,y) = (21,10)
            term.MultiClickSelection({ 21, 10 }, Terminal::SelectionExpansion::Word);

            // Simulate move to (x,y) = (5,10)
            //
            // buffer: doubleClickMe dragThroughHere
            //         ^                ^
            //       finish            start
            term.SetSelectionEnd({ 5, 10 });

            // Validate selection area
            ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
        }

        TEST_METHOD(TripleClick_GeneralCase)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Validate selection area
            ValidateSingleRowSelection(term, til::inclusive_rect({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(TripleClickDrag_Horizontal)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Simulate move to (x,y) = (7,10)
            term.SetSelectionEnd({ 7, 10 });

            // Validate selection area
            ValidateSingleRowSelection(term, til::inclusive_rect({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(TripleClickDrag_Vertical)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = til::point{ 5, 10 };
            term.MultiClickSelection(clickPos, Terminal::SelectionExpansion::Line);

            // Simulate move to (x,y) = (5,11)
            term.SetSelectionEnd({ 5, 11 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(2));

            // verify first selection rect
            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, 10, 99, 10 }));

            // verify second selection rect
            selection = term.GetViewport().ConvertToOrigin(selectionRects.at(1)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, til::inclusive_rect({ 0, 11, 99, 11 }));
        }

        TEST_METHOD(ShiftClick)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            term.GetTextBuffer().GetCursor().SetPosition({ 4, 10 });
            term.Write(text);

            // Step 1: Create a selection on "doubleClickMe"
            {
                // Simulate double click at (x,y) = (5,10)
                term.MultiClickSelection({ 5, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe" selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 16, 10 }));
            }

            // Step 2: Shift+Click to "dragThroughHere"
            {
                // Simulate Shift+Click at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^
                //       start            finish
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area: "doubleClickMe drag" selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 21, 10 }));
            }

            // Step 3: Shift+Double-Click at "dragThroughHere"
            {
                // Simulate Shift+DoubleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^          ^
                //       start            click      finish
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
            }

            // Step 4: Shift+Triple-Click at "dragThroughHere"
            {
                // Simulate Shift+TripleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere     |
                //         ^                ^                ^
                //       start            click            finish (boundary)
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Line);

                // Validate selection area: "doubleClickMe dragThroughHere..." selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 99, 10 }));
            }

            // Step 5: Shift+Double-Click at "dragThroughHere"
            {
                // Simulate Shift+DoubleClick at (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^          ^
                //       start            click      finish
                term.SetSelectionEnd({ 21, 10 }, Terminal::SelectionExpansion::Word);

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
            }

            // Step 6: Drag past "dragThroughHere"
            {
                // Simulate drag to (x,y) = (35,10)
                // Since we were preceded by a double-click, we're in "word" expansion mode
                //
                // buffer: doubleClickMe dragThroughHere     |
                //         ^                                 ^
                //       start                             finish (boundary)
                term.SetSelectionEnd({ 35, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere..." selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 99, 10 }));
            }

            // Step 6: Drag back to "dragThroughHere"
            {
                // Simulate drag to (x,y) = (21,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                ^          ^
                //       start             drag      finish
                term.SetSelectionEnd({ 21, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere" selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
            }

            // Step 7: Drag within "dragThroughHere"
            {
                // Simulate drag to (x,y) = (25,10)
                //
                // buffer: doubleClickMe dragThroughHere
                //         ^                    ^      ^
                //       start                 drag  finish
                term.SetSelectionEnd({ 25, 10 });

                // Validate selection area: "doubleClickMe dragThroughHere" still selected
                ValidateSingleRowSelection(term, til::inclusive_rect({ 4, 10, 32, 10 }));
            }
        }

        TEST_METHOD(Pivot)
        {
            Terminal term;
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            // Step 1: Create a selection
            {
                // (10,10) to (20, 10)
                term.SelectNewRegion({ 10, 10 }, { 20, 10 });

                // Validate selection area
                ValidateSingleRowSelection(term, til::inclusive_rect({ 10, 10, 20, 10 }));
            }

            // Step 2: Drag to (5,10)
            {
                term.SetSelectionEnd({ 5, 10 });

                // Validate selection area
                // NOTE: Pivot should be (10, 10)
                ValidateSingleRowSelection(term, til::inclusive_rect({ 5, 10, 10, 10 }));
            }

            // Step 3: Drag back to (20,10)
            {
                term.SetSelectionEnd({ 20, 10 });

                // Validate selection area
                // NOTE: Pivot should still be (10, 10)
                ValidateSingleRowSelection(term, til::inclusive_rect({ 10, 10, 20, 10 }));
            }

            // Step 4: Shift+Click at (5,10)
            {
                term.SetSelectionEnd({ 5, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area
                // NOTE: Pivot should still be (10, 10)
                ValidateSingleRowSelection(term, til::inclusive_rect({ 5, 10, 10, 10 }));
            }

            // Step 5: Shift+Click back at (20,10)
            {
                term.SetSelectionEnd({ 20, 10 }, Terminal::SelectionExpansion::Char);

                // Validate selection area
                // NOTE: Pivot should still be (10, 10)
                ValidateSingleRowSelection(term, til::inclusive_rect({ 10, 10, 20, 10 }));
            }
        }
    };
}
