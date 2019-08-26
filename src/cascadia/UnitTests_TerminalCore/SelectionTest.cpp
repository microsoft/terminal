/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* This File was generated using the VisualTAEF C++ Project Wizard.
* Class Name: SelectionTest
*/
#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../cascadia/UnitTests_TerminalCore/MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace winrt::Microsoft::Terminal::Settings;

namespace TerminalCoreUnitTests
{
    class SelectionTest
    {
        TEST_CLASS(SelectionTest);

        TEST_METHOD(SelectUnit)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 10, 5, 10 }));
        }

        TEST_METHOD(SelectArea)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            SHORT rowValue = 10;

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetEndSelectionPosition({ 15, 20 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            SHORT rightBoundary = viewport.RightInclusive();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    // Verify top line
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 10, rightBoundary, 10 }));
                }
                else if (rowValue == 20)
                {
                    // Verify bottom line
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 20, 15, 20 }));
                }
                else
                {
                    // Verify other lines (full)
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, rowValue, rightBoundary, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(SelectToOutOfBounds)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 10, 10 }, 0, emptyRT);

            auto viewport = term.GetViewport();
            const SHORT leftBoundary = 0;
            const SHORT rightBoundary = viewport.RightInclusive();

            // Simulate click at (x,y) = (5,5)
            term.SetSelectionAnchor({ 5, 5 });

            // Case 1: Move out of right boundary
            term.SetEndSelectionPosition({ 20, 5 });
            {
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));
                auto selection = viewport.ConvertToOrigin(selectionRects[0]).ToInclusive();

                VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 5, rightBoundary, 5 }));
            }

            // Case 2: Move out of left boundary
            term.SetEndSelectionPosition({ -20, 5 });
            {
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));
                auto selection = viewport.ConvertToOrigin(selectionRects[0]).ToInclusive();

                VERIFY_ARE_EQUAL(selection, SMALL_RECT({ leftBoundary, 5, 5, 5 }));
            }

            // Case 3: Move out of top boundary
            term.SetEndSelectionPosition({ 5, -20 });
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
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, rowValue, rightBoundary, rowValue }));
                    }
                    else if (rowValue == 5)
                    {
                        // Verify last line
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ leftBoundary, rowValue, 5, rowValue }));
                    }
                    else
                    {
                        // Verify other lines (full)
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ leftBoundary, rowValue, rightBoundary, rowValue }));
                    }
                }
            }

            // Case 4: Move out of bottom boundary
            term.SetEndSelectionPosition({ 5, 20 });
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
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 5, rightBoundary, 5 }));
                    }
                    else if (rowValue == 9)
                    {
                        // Verify bottom line
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ leftBoundary, rowValue, 5, rowValue }));
                    }
                    else
                    {
                        // Verify other lines (full)
                        VERIFY_ARE_EQUAL(selection, SMALL_RECT({ leftBoundary, rowValue, rightBoundary, rowValue }));
                    }
                }
            }
        }

        TEST_METHOD(SelectBoxArea)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            SHORT rowValue = 10;

            // Simulate ALT + click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });
            term.SetBoxSelection(true);

            // Simulate move to (x,y) = (15,20)
            term.SetEndSelectionPosition({ 15, 20 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                // Verify all lines
                VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, rowValue, 15, rowValue }));

                rowValue++;
            }
        }

        TEST_METHOD(SelectAreaAfterScroll)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            SHORT scrollbackLines = 5;
            term.Create({ 100, 100 }, scrollbackLines, emptyRT);

            // Used for two things:
            //    - click y-pos
            //    - keep track of row we're verifying
            SHORT rowValue = 10;

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, rowValue });

            // Simulate move to (x,y) = (15,20)
            term.SetEndSelectionPosition({ 15, 20 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(11));

            auto viewport = term.GetViewport();
            SHORT rightBoundary = viewport.RightInclusive();
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    // Verify top line
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 10, rightBoundary, 10 }));
                }
                else if (rowValue == 20)
                {
                    // Verify bottom line
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 20, 15, 20 }));
                }
                else
                {
                    // Verify other lines (full)
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, rowValue, rightBoundary, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(SelectWideGlyph_Trailing)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.SetCursorPosition(4, 10);
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.SetSelectionAnchor(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            // Selection should expand one to the left to get the leading half of the wide glyph
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, 10, 5, 10 }));
        }

        TEST_METHOD(SelectWideGlyph_Leading)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.SetCursorPosition(4, 10);
            term.Write(burrito);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 4, 10 };
            term.SetSelectionAnchor(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            // Selection should expand one to the left to get the leading half of the wide glyph
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, 10, 5, 10 }));
        }

        TEST_METHOD(SelectWideGlyphsInBoxSelection)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // This is the burrito emoji
            // It's encoded in UTF-16, as needed by the buffer.
            const auto burrito = L"\xD83C\xDF2F";

            // Insert wide glyph at position (4,10)
            term.SetCursorPosition(4, 10);
            term.Write(burrito);

            // Insert wide glyph at position (7,11)
            term.SetCursorPosition(7, 11);
            term.Write(burrito);

            // Simulate ALT + click at (x,y) = (5,8)
            term.SetSelectionAnchor({ 5, 8 });
            term.SetBoxSelection(true);

            // Simulate move to (x,y) = (7,12)
            term.SetEndSelectionPosition({ 7, 12 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(5));

            auto viewport = term.GetViewport();
            SHORT rowValue = 8;
            for (auto selectionRect : selectionRects)
            {
                auto selection = viewport.ConvertToOrigin(selectionRect).ToInclusive();

                if (rowValue == 10)
                {
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, rowValue, 7, rowValue }));
                }
                else if (rowValue == 11)
                {
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, rowValue, 8, rowValue }));
                }
                else
                {
                    // Verify all lines
                    VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, rowValue, 7, rowValue }));
                }

                rowValue++;
            }
        }

        TEST_METHOD(DoubleClick_GeneralCase)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe";
            term.SetCursorPosition(4, 10);
            term.Write(text);

            // Simulate double click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.DoubleClickSelection(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, 10, (4 + gsl::narrow<SHORT>(text.size()) - 1), 10 }));
        }

        TEST_METHOD(DoubleClick_Delimiter)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.DoubleClickSelection(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(DoubleClickDrag_Right)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (4,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            term.SetCursorPosition(4, 10);
            term.Write(text);

            // Simulate double click at (x,y) = (5,10)
            term.DoubleClickSelection({ 5, 10 });

            // Simulate move to (x,y) = (21,10)
            //
            // buffer: doubleClickMe dragThroughHere
            //         ^                ^
            //       start            finish
            term.SetEndSelectionPosition({ 21, 10 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, 10, 32, 10 }));
        }

        TEST_METHOD(DoubleClickDrag_Left)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // set word delimiters for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            term.UpdateSettings(settings);

            // Insert text at position (21,10)
            const std::wstring_view text = L"doubleClickMe dragThroughHere";
            term.SetCursorPosition(4, 10);
            term.Write(text);

            // Simulate double click at (x,y) = (21,10)
            term.DoubleClickSelection({ 21, 10 });

            // Simulate move to (x,y) = (5,10)
            //
            // buffer: doubleClickMe dragThroughHere
            //         ^                ^
            //       finish            start
            term.SetEndSelectionPosition({ 5, 10 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 4, 10, 32, 10 }));
        }

        TEST_METHOD(TripleClick_GeneralCase)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.TripleClickSelection(clickPos);

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(TripleClickDrag_Horizontal)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.TripleClickSelection(clickPos);

            // Simulate move to (x,y) = (7,10)
            term.SetEndSelectionPosition({ 7, 10 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));

            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 10, 99, 10 }));
        }

        TEST_METHOD(TripleClickDrag_Vertical)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // Simulate click at (x,y) = (5,10)
            auto clickPos = COORD{ 5, 10 };
            term.TripleClickSelection(clickPos);

            // Simulate move to (x,y) = (5,11)
            term.SetEndSelectionPosition({ 5, 11 });

            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionRects = term.GetSelectionRects();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(2));

            // verify first selection rect
            auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 10, 99, 10 }));

            // verify second selection rect
            selection = term.GetViewport().ConvertToOrigin(selectionRects.at(1)).ToInclusive();
            VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 0, 11, 99, 11 }));
        }

        TEST_METHOD(CopyOnSelect)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            // set copyOnSelect for terminal
            auto settings = winrt::make<MockTermSettings>(0, 100, 100);
            settings.CopyOnSelect(true);
            term.UpdateSettings(settings);

            // Simulate click at (x,y) = (5,10)
            term.SetSelectionAnchor({ 5, 10 });

            // Simulate move to (x,y) = (5,10)
            // (So, no movement)
            term.SetEndSelectionPosition({ 5, 10 });

            // Case 1: single cell selection not allowed
            {
                // Simulate renderer calling TriggerSelection and acquiring selection area
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(0));

                // single cell selection should not be allowed
                // thus, selection is NOT active
                VERIFY_IS_FALSE(term.IsSelectionActive());
            }

            // Case 2: move off of single cell
            term.SetEndSelectionPosition({ 6, 10 });
            { // Simulate renderer calling TriggerSelection and acquiring selection area
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));
                auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
                VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 10, 6, 10 }));
                VERIFY_IS_TRUE(term.IsSelectionActive());
            }

            // Case 3: move back onto single cell (now allowed)
            term.SetEndSelectionPosition({ 5, 10 });
            { // Simulate renderer calling TriggerSelection and acquiring selection area
                auto selectionRects = term.GetSelectionRects();

                // Validate selection area
                VERIFY_ARE_EQUAL(selectionRects.size(), static_cast<size_t>(1));
                auto selection = term.GetViewport().ConvertToOrigin(selectionRects.at(0)).ToInclusive();
                VERIFY_ARE_EQUAL(selection, SMALL_RECT({ 5, 10, 5, 10 }));

                // single cell selection should now be allowed
                VERIFY_IS_TRUE(term.IsSelectionActive());
            }
        }
    };
}
