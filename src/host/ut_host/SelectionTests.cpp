// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"

#include "selection.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class SelectionTests
{
    TEST_CLASS(SelectionTests);

    CommonState* m_state;
    Selection* m_pSelection;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalScreenBuffer();

        m_pSelection = &Selection::Instance();
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_pSelection = nullptr;

        m_state->CleanupGlobalScreenBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        return true;
    }

    void VerifyGetSelectionSpans_BoxMode()
    {
        const auto selectionSpans = m_pSelection->GetSelectionSpans();
        const UINT cRectanglesExpected = m_pSelection->_d->srSelectionRect.bottom - m_pSelection->_d->srSelectionRect.top + 1;

        if (VERIFY_ARE_EQUAL(cRectanglesExpected, selectionSpans.size()))
        {
            for (auto iRect = 0; iRect < gsl::narrow<int>(selectionSpans.size()); iRect++)
            {
                // ensure each rectangle is exactly the width requested (block selection)
                const auto& span = selectionSpans[iRect];

                const auto sRectangleLineNumber = (til::CoordType)iRect + m_pSelection->_d->srSelectionRect.top;

                VERIFY_ARE_EQUAL(span.start.y, sRectangleLineNumber);
                VERIFY_ARE_EQUAL(span.end.y, sRectangleLineNumber);

                VERIFY_ARE_EQUAL(span.start.x, m_pSelection->_d->srSelectionRect.left);
                VERIFY_ARE_EQUAL(span.end.x, m_pSelection->_d->srSelectionRect.right + 1);
            }
        }
    }

    TEST_METHOD(TestGetSelectionSpans_BoxMode)
    {
        {
            auto selection{ m_pSelection->_d.write() };
            selection->fSelectionVisible = true;

            // set selection region
            selection->srSelectionRect.top = 0;
            selection->srSelectionRect.bottom = 3;
            selection->srSelectionRect.left = 1;
            selection->srSelectionRect.right = 10;

            // #1 top-left to bottom right selection first
            selection->coordSelectionAnchor.x = selection->srSelectionRect.left;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.top;

            // A. false/false for the selection modes should mean box selection
            selection->fLineSelection = false;
            selection->fUseAlternateSelection = false;

            VerifyGetSelectionSpans_BoxMode();
        }

        {
            auto selection{ m_pSelection->_d.write() };
            // B. true/true for the selection modes should also mean box selection
            selection->fLineSelection = true;
            selection->fUseAlternateSelection = true;

            VerifyGetSelectionSpans_BoxMode();
        }

        {
            auto selection{ m_pSelection->_d.write() };
            // now try the other 3 configurations of box region.
            // #2 top-right to bottom-left selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.right;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.top;

            VerifyGetSelectionSpans_BoxMode();
        }

        {
            auto selection{ m_pSelection->_d.write() };
            // #3 bottom-left to top-right selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.left;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.bottom;

            VerifyGetSelectionSpans_BoxMode();
        }

        {
            auto selection{ m_pSelection->_d.write() };

            // #4 bottom-right to top-left selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.right;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.bottom;

            VerifyGetSelectionSpans_BoxMode();
        }
    }

    void VerifyGetSelectionSpans_LineMode(const til::point inclusiveStart, const til::point inclusiveEnd)
    {
        const auto selectionSpans = m_pSelection->GetSelectionSpans();

        if (VERIFY_ARE_EQUAL(1u, selectionSpans.size()))
        {
            auto& span{ selectionSpans[0] };
            VERIFY_ARE_EQUAL(inclusiveStart, span.start, L"start");

            const til::point exclusiveEnd{ inclusiveEnd.x + 1, inclusiveEnd.y };
            VERIFY_ARE_EQUAL(exclusiveEnd, span.end, L"end");
        }
    }

    // All of the logic tested herein is trying to determine where the selection
    // must have started, given a rectangle and the point where the mouse was last seen.
    TEST_METHOD(TestGetSelectionSpans_LineMode)
    {
        {
            auto selection{ m_pSelection->_d.write() };
            selection->fSelectionVisible = true;

            // Part I: Multiple line selection
            // set selection region
            selection->srSelectionRect.top = 0;
            selection->srSelectionRect.bottom = 3;
            selection->srSelectionRect.left = 1;
            selection->srSelectionRect.right = 10;

            /*
                   |  RECT   |
                   0123456789ABCDEF
                --0+---------+
                  1|         |
                  2|         |
                --3+---------+
                  4
            */

            // #1 top-left to bottom right selection first
            selection->coordSelectionAnchor.x = selection->srSelectionRect.left;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.top;

            // A. true/false for the selection modes should mean line selection
            selection->fLineSelection = true;
            selection->fUseAlternateSelection = false;

            /*
                Mouse at 0,0; therefore, the selection "begins" at 3,10
                Selection extends to bottom right corner of rectangle

                   |  RECT   |
                   0123456789ABCDEF
                --0*#########*#####
                  1################
                  2################
                --3*#########*
                  4
            */
            VerifyGetSelectionSpans_LineMode({ 1, 0 }, { 10, 3 });
        }

        {
            auto selection{ m_pSelection->_d.write() };
            // B. false/true for the selection modes should also mean line selection
            selection->fLineSelection = false;
            selection->fUseAlternateSelection = true;

            // Same as above.
            VerifyGetSelectionSpans_LineMode({ 1, 0 }, { 10, 3 });
        }

        {
            auto selection{ m_pSelection->_d.write() };
            // now try the other 3 configurations of box region.
            // #2 top-right to bottom-left selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.right;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.top;

            /*
                Mouse at 0,10; therefore, the selection must have started at 3,0
                Selection does not include bottom-most line

                   |  RECT   |
                   0123456789ABCDEF
                --0+         *#####
                  1################
                  2################
                --3*         +
                  4
            */

            VerifyGetSelectionSpans_LineMode({ 10, 0 }, { 1, 3 });
        }

        {
            auto selection{ m_pSelection->_d.write() };

            // #3 bottom-left to top-right selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.left;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.bottom;

            /*
                Mouse at 3,1; therefore, the selection must have started at 0,10
                Selection extends from top right to bottom left

                   |  RECT   |
                   0123456789ABCDEF
                --0+         *#####
                  1################
                  2################
                --3*         +
                  4
            */
            VerifyGetSelectionSpans_LineMode({ 10, 0 }, { 1, 3 });
        }

        {
            auto selection{ m_pSelection->_d.write() };

            // #4 bottom-right to top-left selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.right;
            selection->coordSelectionAnchor.y = selection->srSelectionRect.bottom;

            /*
                Mouse at 3,10; therefore, the selection must have started at 0,0
                Just like case #1, selection covers all lines and top left/bottom right of rect.

                   |  RECT   |
                   0123456789ABCDEF
                --0*#########*#####
                  1################
                  2################
                --3*#########*
                  4
            */
            VerifyGetSelectionSpans_LineMode({ 1, 0 }, { 10, 3 });
        }

        {
            auto selection{ m_pSelection->_d.write() };

            // Part II: Single line selection
            selection->srSelectionRect.top = 2;
            selection->srSelectionRect.bottom = 2;
            selection->srSelectionRect.left = 1;
            selection->srSelectionRect.right = 10;

            // #1: left to right selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.left;
            VERIFY_IS_TRUE(selection->srSelectionRect.bottom == selection->srSelectionRect.top);
            selection->coordSelectionAnchor.y = selection->srSelectionRect.bottom;

            VerifyGetSelectionSpans_LineMode({ 1, 2 }, { 10, 2 });
        }

        {
            auto selection{ m_pSelection->_d.write() };

            // #2: right to left selection
            selection->coordSelectionAnchor.x = selection->srSelectionRect.right;
            VERIFY_IS_TRUE(selection->srSelectionRect.bottom == selection->srSelectionRect.top);
            selection->coordSelectionAnchor.y = selection->srSelectionRect.top;

            VerifyGetSelectionSpans_LineMode({ 1, 2 }, { 10, 2 });
        }
    }

    void TestBisectSelectionDelta(til::CoordType sTargetX, til::CoordType sTargetY, til::CoordType sLength, til::CoordType sDeltaLeft, til::CoordType sDeltaRight)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& screenInfo = gci.GetActiveOutputBuffer();

        til::CoordType sStringLength;
        til::point coordTargetPoint;
        til::inclusive_rect srSelection;
        til::inclusive_rect srOriginal;

        sStringLength = sLength;
        coordTargetPoint.x = sTargetX;
        coordTargetPoint.y = sTargetY;

        // selection area is always one row at a time so top/bottom = Y = row position
        srSelection.top = srSelection.bottom = coordTargetPoint.y;

        // selection rectangle starts from the target and goes for the length requested
        srSelection.left = coordTargetPoint.x;
        srSelection.right = coordTargetPoint.x + sStringLength;

        // save original for comparison
        srOriginal.top = srSelection.top;
        srOriginal.bottom = srSelection.bottom;
        srOriginal.left = srSelection.left;
        srOriginal.right = srSelection.right;

        til::point startPos{ sTargetX, sTargetY };
        til::point endPos{ sTargetX + sLength, sTargetY };
        const auto selectionRects = screenInfo.GetTextBuffer().GetTextRects(startPos, endPos, false, false);

        VERIFY_ARE_EQUAL(static_cast<size_t>(1), selectionRects.size());
        srSelection = selectionRects.at(0);

        VERIFY_ARE_EQUAL(srOriginal.top, srSelection.top);
        VERIFY_ARE_EQUAL(srOriginal.bottom, srSelection.bottom);
        VERIFY_ARE_EQUAL(srOriginal.left + sDeltaLeft, srSelection.left);
        VERIFY_ARE_EQUAL(srOriginal.right + sDeltaRight, srSelection.right);
    }
};

class SelectionInputTests
{
    TEST_CLASS(SelectionInputTests);

    CommonState* m_state;
    CommandHistory* m_pHistory;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();
        m_pHistory = CommandHistory::s_Allocate(L"cmd.exe", nullptr);
        if (!m_pHistory)
        {
            return false;
        }
        // History must be prepared before COOKED_READ (as it uses s_Find to get at it)

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        CommandHistory::s_Free(nullptr);
        m_pHistory = nullptr;
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD(TestWordByWordPrevious)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = gci.GetActiveOutputBuffer();

        const std::wstring text(L"this is some test text.");
        screenInfo.Write(OutputCellIterator(text));

        // Get the left and right side of the text we inserted (right is one past the end)
        const til::point left;
        const til::point right{ gsl::narrow<til::CoordType>(text.length()), 0 };

        // Get the selection instance and buffer size
        auto& sel = Selection::Instance();
        const auto bufferSize = screenInfo.GetBufferSize();

        // The anchor is where the selection started from.
        const auto anchor = right;

        // The point is the "other end" of the anchor forming the rectangle of what is covered.
        // It starts at the same spot as the anchor to represent the initial 1x1 selection.
        auto point = anchor;

        // Walk through the sequence in reverse extending the sequence by one word each time to the left.
        // The anchor is always the end of the line and the selection just gets bigger.
        do
        {
            // We expect the result to be left of where we started.
            // It will point at the character just right of the space (or the beginning of the line).
            auto resultExpected = point;

            do
            {
                resultExpected.x--;
            } while (resultExpected.x > 0 && text.at(resultExpected.x - 1) != UNICODE_SPACE);

            point = sel.WordByWordSelection(true, bufferSize, anchor, point);

            VERIFY_ARE_EQUAL(resultExpected, point);

        } while (point.x > left.x);
    }

    TEST_METHOD(TestWordByWordNext)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
        END_TEST_METHOD_PROPERTIES()

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = gci.GetActiveOutputBuffer();

        const std::wstring text(L"this is some test text.");
        screenInfo.Write(OutputCellIterator(text));

        // Get the left and right side of the text we inserted (right is one past the end)
        const til::point left;
        const til::point right = { gsl::narrow<til::CoordType>(text.length()), 0 };

        // Get the selection instance and buffer size
        auto& sel = Selection::Instance();
        const auto bufferSize = screenInfo.GetBufferSize();

        // The anchor is where the selection started from.
        const auto anchor = left;

        // The point is the "other end" of the anchor forming the rectangle of what is covered.
        // It starts at the same spot as the anchor to represent the initial 1x1 selection.
        auto point = anchor;

        // Walk through the sequence forward extending the sequence by one word each time to the right.
        // The anchor is always the end of the line and the selection just gets bigger.
        do
        {
            // We expect the result to be right of where we started.

            auto resultExpected = point;

            do
            {
                resultExpected.x++;
            } while (resultExpected.x + 1 < right.x && text.at(resultExpected.x + 1) != UNICODE_SPACE);
            resultExpected.x++;

            // when we reach the end, word by word selection will seek forward to the end of the buffer, so update
            // the expected to the end in that circumstance
            if (resultExpected.x >= right.x)
            {
                resultExpected.x = bufferSize.RightInclusive();
                resultExpected.y = bufferSize.BottomInclusive();
            }

            point = sel.WordByWordSelection(false, bufferSize, anchor, point);

            VERIFY_ARE_EQUAL(resultExpected, point);

        } while (point.y < bufferSize.BottomInclusive()); // stop once we've advanced to a point on the bottom row of the buffer.
    }
};
