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
    // This is a view model class that extends the Command model,
    // by managing a highlighted text that is computed by matching search filter characters to command name
    FilteredCommand::FilteredCommand(Microsoft::Terminal::Settings::Model::Command const& command) :
        _Command(command),
        _Filter(L"")
    {
        _HighlightedName = _computeHighlightedName();

        // Recompute the highlighted name if the command name changes
        _commandChangedRevoker = _Command.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](Windows::Foundation::IInspectable const& /*sender*/, Data::PropertyChangedEventArgs const& e) {
            auto filteredCommand{ weakThis.get() };
            if (filteredCommand && e.PropertyName() == L"Name")
            {
                filteredCommand->HighlightedName(filteredCommand->_computeHighlightedName());
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
        }
    }

    // Method Description:
    // - Looks up the filter characters within the command name.
    // Iterating through the filter and the command name it tries to associate the next filter character
    // with the first appearance of this character in the command name suffix.
    //
    // E.g., for filter="c l t s" and name="close all tabs after this", the match will be "CLose TabS after this".
    //
    // The command name is then split into segments (groupings of matched and non matched characters).
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
        auto commandName = _Command.Name();
        bool isProcessingMatchedSegment = false;
        uint32_t nextOffsetToReport = 0;
        uint32_t currentOffset = 0;

        for (const auto searchChar : _Filter)
        {
            const auto lowerCaseSearchChar = std::towlower(searchChar);
            while (true)
            {
                if (currentOffset == commandName.size())
                {
                    // There are still unmatched filter characters but we finished scanning the name.
                    // This should not happen, if we are processing only names that matched the filter
                    throw winrt::hresult_invalid_argument();
                }

                auto isCurrentCharMatched = std::towlower(commandName[currentOffset]) == lowerCaseSearchChar;
                if (isProcessingMatchedSegment != isCurrentCharMatched)
                {
                    // We reached the end of the region (matched character came after a series of unmatched or vice versa).
                    // Conclude the segment and add it to the list.
                    // Skip segment if it is empty (might happen when the first character of the name is matched)
                    auto sizeToReport = currentOffset - nextOffsetToReport;
                    if (sizeToReport > 0)
                    {
                        winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
                        auto highlightedSegment = winrt::make_self<HighlightedTextSegment>(segment, isProcessingMatchedSegment);
                        segments.Append(*highlightedSegment);
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

        // Either the filter or the command name were fully processed.
        // If we were in the middle of the matched segment - add it.
        if (isProcessingMatchedSegment)
        {
            auto sizeToReport = currentOffset - nextOffsetToReport;
            if (sizeToReport > 0)
            {
                winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
                auto highlightedSegment = winrt::make_self<HighlightedTextSegment>(segment, true);
                segments.Append(*highlightedSegment);
                nextOffsetToReport = currentOffset;
            }
        }

        // Now create a segment for all remaining characters.
        // We will have remaining characters as long as the filter is shorter than the command name.
        auto sizeToReport = commandName.size() - nextOffsetToReport;
        if (sizeToReport > 0)
        {
            winrt::hstring segment{ commandName.data() + nextOffsetToReport, sizeToReport };
            auto highlightedSegment = winrt::make_self<HighlightedTextSegment>(segment, false);
            segments.Append(*highlightedSegment);
        }

        return *winrt::make_self<winrt::TerminalApp::implementation::HighlightedText>(segments);
    }
}
