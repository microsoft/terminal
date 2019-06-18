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
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;

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
    };
}
