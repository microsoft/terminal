// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../TerminalApp/TabDragDropHelpers.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests
{
    class TabDragDropHelpersTests
    {
        BEGIN_TEST_CLASS(TabDragDropHelpersTests)
        END_TEST_CLASS()

        TEST_METHOD(ComputeAdjustedInsertIndexShiftsForRemovedTabs);
        TEST_METHOD(ComputeAdjustedInsertIndexClampsToTabCount);
        TEST_METHOD(ComputeAdjustedInsertIndexNoShiftWhenDraggedTabsOnOrAfterInsert);
        TEST_METHOD(IsNoOpMoveOnlyForContiguousBlockAtInsertIndex);
        TEST_METHOD(IsNoOpMoveFalseForEmptySelection);
    };

    void TabDragDropHelpersTests::ComputeAdjustedInsertIndexShiftsForRemovedTabs()
    {
        const std::vector<uint32_t> dragged{ 1, 3 };
        const auto actual = TerminalApp::TabDragDrop::ComputeAdjustedInsertIndex(4, 5, dragged);
        VERIFY_ARE_EQUAL(3u, actual);
    }

    void TabDragDropHelpersTests::ComputeAdjustedInsertIndexClampsToTabCount()
    {
        const std::vector<uint32_t> dragged{ 0 };
        const auto actual = TerminalApp::TabDragDrop::ComputeAdjustedInsertIndex(99, 5, dragged);
        VERIFY_ARE_EQUAL(4u, actual);
    }

    void TabDragDropHelpersTests::ComputeAdjustedInsertIndexNoShiftWhenDraggedTabsOnOrAfterInsert()
    {
        const std::vector<uint32_t> dragged{ 3, 4 };
        const auto actual = TerminalApp::TabDragDrop::ComputeAdjustedInsertIndex(3, 5, dragged);
        VERIFY_ARE_EQUAL(3u, actual);
    }

    void TabDragDropHelpersTests::IsNoOpMoveOnlyForContiguousBlockAtInsertIndex()
    {
        const std::vector<uint32_t> contiguous{ 2, 3, 4 };
        const std::vector<uint32_t> nonContiguous{ 1, 3, 4 };

        VERIFY_IS_TRUE(TerminalApp::TabDragDrop::IsNoOpMove(contiguous, 2));
        VERIFY_IS_FALSE(TerminalApp::TabDragDrop::IsNoOpMove(contiguous, 1));
        VERIFY_IS_FALSE(TerminalApp::TabDragDrop::IsNoOpMove(nonContiguous, 1));
    }

    void TabDragDropHelpersTests::IsNoOpMoveFalseForEmptySelection()
    {
        const std::vector<uint32_t> empty{};
        VERIFY_IS_FALSE(TerminalApp::TabDragDrop::IsNoOpMove(empty, 0));
    }
}
