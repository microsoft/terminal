// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace TerminalApp::TabDragDrop
{
    // Computes the destination insertion index after removing the dragged tabs from
    // their current positions. `sortedDraggedIndices` must be sorted ascending.
    inline uint32_t ComputeAdjustedInsertIndex(const uint32_t suggestedIndex,
                                               const uint32_t tabCount,
                                               const std::vector<uint32_t>& sortedDraggedIndices)
    {
        auto insertIndex = std::min(suggestedIndex, tabCount);
        for (const auto index : sortedDraggedIndices)
        {
            if (index < insertIndex)
            {
                insertIndex--;
            }
        }

        return insertIndex;
    }

    inline bool AreContiguous(const std::vector<uint32_t>& sortedIndices)
    {
        if (sortedIndices.empty())
        {
            return false;
        }

        return std::adjacent_find(sortedIndices.begin(), sortedIndices.end(), [](const auto lhs, const auto rhs) {
            return rhs != lhs + 1;
        }) == sortedIndices.end();
    }

    inline bool IsNoOpMove(const std::vector<uint32_t>& sortedDraggedIndices, const uint32_t insertIndex)
    {
        return AreContiguous(sortedDraggedIndices) && sortedDraggedIndices.front() == insertIndex;
    }
}
