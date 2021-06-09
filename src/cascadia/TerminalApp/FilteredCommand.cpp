// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
#include "HighlightedText.h"
#include <LibraryResources.h>

#include "FilteredCommand.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    // This class is a wrapper of PaletteItem, that is used as an item of a filterable list in CommandPalette.
    // It manages a highlighted text that is computed by matching search filter characters to item name
    FilteredCommand::FilteredCommand(winrt::TerminalApp::PaletteItem const& item) :
        _Item(item),
        _Filter(L""),
        _Weight(0)
    {
        _HighlightedName = _computeHighlightedName();

        // Recompute the highlighted name if the item name changes
        _itemChangedRevoker = _Item.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& /*sender*/, auto& e) {
            auto filteredCommand{ weakThis.get() };
            if (filteredCommand && e.PropertyName() == L"Name")
            {
                filteredCommand->HighlightedName(filteredCommand->_computeHighlightedName());
                filteredCommand->Weight(filteredCommand->_computeWeight());
            }
        });
    }

    void FilteredCommand::UpdateFilter(winrt::hstring const& filter)
    {
        // If the filter was not changed we want to prevent the re-computation of matching
        // that might result in triggering a notification event
        if (filter != _Filter)
        {
            Filter(filter);
            HighlightedName(_computeHighlightedName());
            Weight(_computeWeight());
        }
    }

    // Method Description:
    // - Looks up the filter characters within the item name.
    // Iterating through the filter and the item name it tries to associate the next filter character
    // with the first appearance of this character in the item name suffix.
    //
    // E.g., for filter="c l t s" and name="close all tabs after this", the match will be "CLose TabS after this".
    //
    // The item name is then split into segments (groupings of matched and non matched characters).
    //
    // E.g., the segments were the example above will be "CL", "ose ", "T", "ab", "S", "after this".
    //
    // The segments matching the filter characters are marked as highlighted.
    //
    // E.g., ("CL", true) ("ose ", false), ("T", true), ("ab", false), ("S", true), ("after this", false)
    //
    // TODO: we probably need to merge this logic with _getWeight computation?
    //
    // Return Value:
    // - The HighlightedText object initialized with the segments computed according to the algorithm above.
    winrt::TerminalApp::HighlightedText FilteredCommand::_computeHighlightedName()
    {
        const auto segments = winrt::single_threaded_observable_vector<winrt::TerminalApp::HighlightedTextSegment>();
        auto commandName = _Item.Name();
        bool isProcessingMatchedSegment = false;
        uint32_t nextOffsetToReport = 0;
        uint32_t currentOffset = 0;

        for (const auto searchChar : _Filter)
        {
            const WCHAR searchCharAsString[] = { searchChar, L'\0' };
            while (true)
            {
                if (currentOffset == commandName.size())
                {
                    // There are still unmatched filter characters but we finished scanning the name.
                    // In this case we return the entire item name as unmatched
                    auto entireNameSegment{ winrt::make<HighlightedTextSegment>(commandName, false) };
                    segments.Clear();
                    segments.Append(entireNameSegment);
                    return winrt::make<HighlightedText>(segments);
                }

                // GH#9941: search should be locale-aware as well
                // We use the same comparison method as upon sorting to guarantee consistent behavior
                const WCHAR currentCharAsString[] = { commandName[currentOffset], L'\0' };
                auto isCurrentCharMatched = lstrcmpi(searchCharAsString, currentCharAsString) == 0;
                if (isProcessingMatchedSegment != isCurrentCharMatched)
                {
                    // We reached the end of the region (matched character came after a series of unmatched or vice versa).
                    // Conclude the segment and add it to the list.
                    // Skip segment if it is empty (might happen when the first character of the name is matched)
                    auto sizeToReport = currentOffset - nextOffsetToReport;
                    if (sizeToReport > 0)
                    {
                        winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
                        auto highlightedSegment{ winrt::make<HighlightedTextSegment>(segment, isProcessingMatchedSegment) };
                        segments.Append(highlightedSegment);
                        nextOffsetToReport = currentOffset;
                    }
                    isProcessingMatchedSegment = isCurrentCharMatched;
                }

                currentOffset++;

                if (isCurrentCharMatched)
                {
                    // We have matched this filter character, let's move to matching the next filter char
                    break;
                }
            }
        }

        // Either the filter or the item name were fully processed.
        // If we were in the middle of the matched segment - add it.
        if (isProcessingMatchedSegment)
        {
            auto sizeToReport = currentOffset - nextOffsetToReport;
            if (sizeToReport > 0)
            {
                winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
                auto highlightedSegment{ winrt::make<HighlightedTextSegment>(segment, true) };
                segments.Append(highlightedSegment);
                nextOffsetToReport = currentOffset;
            }
        }

        // Now create a segment for all remaining characters.
        // We will have remaining characters as long as the filter is shorter than the item name.
        auto sizeToReport = commandName.size() - nextOffsetToReport;
        if (sizeToReport > 0)
        {
            winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
            auto highlightedSegment{ winrt::make<HighlightedTextSegment>(segment, false) };
            segments.Append(highlightedSegment);
        }

        return winrt::make<HighlightedText>(segments);
    }

    // Function Description:
    // - Calculates a "weighting" by which should be used to order a item
    //   name relative to other names, given a specific search string.
    //   Currently, this is based off of two factors:
    //   * The weight is incremented once for each matched character of the
    //     search text.
    //   * If a matching character from the search text was found at the start
    //     of a word in the name, then we increment the weight again.
    //     * For example, for a search string "sp", we want "Split Pane" to
    //       appear in the list before "Close Pane"
    //   * Consecutive matches will be weighted higher than matches with
    //     characters in between the search characters.
    // - This will return 0 if the item should not be shown. If all the
    //   characters of search text appear in order in `name`, then this function
    //   will return a positive number. There can be any number of characters
    //   separating consecutive characters in searchText.
    //   * For example:
    //      "name": "New Tab"
    //      "name": "Close Tab"
    //      "name": "Close Pane"
    //      "name": "[-] Split Horizontal"
    //      "name": "[ | ] Split Vertical"
    //      "name": "Next Tab"
    //      "name": "Prev Tab"
    //      "name": "Open Settings"
    //      "name": "Open Media Controls"
    //   * "open" should return both "**Open** Settings" and "**Open** Media Controls".
    //   * "Tab" would return "New **Tab**", "Close **Tab**", "Next **Tab**" and "Prev
    //     **Tab**".
    //   * "P" would return "Close **P**ane", "[-] S**p**lit Horizontal", "[ | ]
    //     S**p**lit Vertical", "**P**rev Tab", "O**p**en Settings" and "O**p**en Media
    //     Controls".
    //   * "sv" would return "[ | ] Split Vertical" (by matching the **S** in
    //     "Split", then the **V** in "Vertical").
    // Arguments:
    // - searchText: the string of text to search for in `name`
    // - name: the name to check
    // Return Value:
    // - the relative weight of this match
    int FilteredCommand::_computeWeight()
    {
        int result = 0;
        bool isNextSegmentWordBeginning = true;

        for (const auto& segment : _HighlightedName.Segments())
        {
            const auto& segmentText = segment.TextSegment();
            const auto segmentSize = segmentText.size();

            if (segment.IsHighlighted())
            {
                // Give extra point for each consecutive match
                result += (segmentSize <= 1) ? segmentSize : 1 + 2 * (segmentSize - 1);

                // Give extra point if this segment is at the beginning of a word
                if (isNextSegmentWordBeginning)
                {
                    result++;
                }
            }

            isNextSegmentWordBeginning = segmentSize > 0 && segmentText[segmentSize - 1] == L' ';
        }

        return result;
    }

    // Function Description:
    // - Implementation of Compare for FilteredCommand interface.
    // Compares first instance of the interface with the second instance, first by weight, then by name.
    // In the case of a tie prefers the first instance.
    // Arguments:
    // - other: another instance of FilteredCommand interface
    // Return Value:
    // - Returns true if the first is "bigger" (aka should appear first)
    int FilteredCommand::Compare(winrt::TerminalApp::FilteredCommand const& first, winrt::TerminalApp::FilteredCommand const& second)
    {
        auto firstWeight{ first.Weight() };
        auto secondWeight{ second.Weight() };

        if (firstWeight == secondWeight)
        {
            std::wstring_view firstName{ first.Item().Name() };
            std::wstring_view secondName{ second.Item().Name() };
            return lstrcmpi(firstName.data(), secondName.data()) < 0;
        }

        return firstWeight > secondWeight;
    }
}
