// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
#include "HighlightedText.h"
#include <LibraryResources.h>
#include "fzf/fzf.h"

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
    FilteredCommand::FilteredCommand(const winrt::TerminalApp::PaletteItem& item)
    {
        // Actually implement the ctor in _constructFilteredCommand
        _constructFilteredCommand(item);
    }

    // We need to actually implement the ctor in a separate helper. This is
    // because we have a FilteredTask class which derives from FilteredCommand.
    // HOWEVER, for cppwinrt ~ r e a s o n s ~, it doesn't actually derive from
    // FilteredCommand directly, so we can't just use the FilteredCommand ctor
    // directly in the base class.
    void FilteredCommand::_constructFilteredCommand(const winrt::TerminalApp::PaletteItem& item)
    {
        _Item = item;
        _Weight = 0;

        _update();

        // Recompute the highlighted name if the item name changes
        _itemChangedRevoker = _Item.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& /*sender*/, auto& e) {
            auto filteredCommand{ weakThis.get() };
            if (filteredCommand && e.PropertyName() == L"Name")
            {
                filteredCommand->_update();
            }
        });
    }

    void FilteredCommand::UpdateFilter(std::shared_ptr<fzf::matcher::Pattern> pattern)
    {
        // If the filter was not changed we want to prevent the re-computation of matching
        // that might result in triggering a notification event
        if (pattern != _pattern)
        {
            _pattern = pattern;
            _update();
        }
    }

    void FilteredCommand::_update()
    {
        std::vector<winrt::TerminalApp::HighlightedTextSegment> segments;
        const auto commandName = _Item.Name();
        int32_t weight = 0;

        if (!_pattern || _pattern->terms.empty())
        {
            segments.emplace_back(winrt::TerminalApp::HighlightedTextSegment(commandName, false));
        }
        else if (auto match = fzf::matcher::Match(commandName, *_pattern.get()); !match)
        {
            segments.emplace_back(winrt::TerminalApp::HighlightedTextSegment(commandName, false));
        }
        else
        {
            auto& matchResult = *match;
            weight = matchResult.Score;

            size_t lastPos = 0;
            for (const auto& run : matchResult.Runs)
            {
                const auto& [start, end] = run;
                if (start > lastPos)
                {
                    hstring nonMatch{ til::safe_slice_abs(commandName, lastPos, start) };
                    segments.emplace_back(winrt::TerminalApp::HighlightedTextSegment(nonMatch, false));
                }

                hstring matchSeg{ til::safe_slice_abs(commandName, start, end + 1) };
                segments.emplace_back(winrt::TerminalApp::HighlightedTextSegment(matchSeg, true));

                lastPos = end + 1;
            }

            if (lastPos < commandName.size())
            {
                hstring tail{ til::safe_slice_abs(commandName, lastPos, SIZE_T_MAX) };
                segments.emplace_back(winrt::TerminalApp::HighlightedTextSegment(tail, false));
            }
        }

        HighlightedName(winrt::make<HighlightedText>(winrt::single_threaded_observable_vector(std::move(segments))));
        Weight(weight);
    }

    // Function Description:
    // - Implementation of Compare for FilteredCommand interface.
    // Compares first instance of the interface with the second instance, first by weight, then by name.
    // In the case of a tie prefers the first instance.
    // Arguments:
    // - other: another instance of FilteredCommand interface
    // Return Value:
    // - Returns true if the first is "bigger" (aka should appear first)
    int FilteredCommand::Compare(const winrt::TerminalApp::FilteredCommand& first, const winrt::TerminalApp::FilteredCommand& second)
    {
        auto firstWeight{ first.Weight() };
        auto secondWeight{ second.Weight() };

        if (firstWeight == secondWeight)
        {
            const auto firstName = first.Item().Name();
            const auto secondName = second.Item().Name();
            return til::compare_linguistic_insensitive(firstName, secondName) < 0;
        }

        return firstWeight > secondWeight;
    }
}
