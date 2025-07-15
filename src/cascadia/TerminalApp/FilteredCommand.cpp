// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
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
    // This class is a wrapper of IPaletteItem, that is used as an item of a filterable list in CommandPalette.
    // It manages a highlighted text that is computed by matching search filter characters to item name
    FilteredCommand::FilteredCommand(const winrt::TerminalApp::IPaletteItem& item)
    {
        // Actually implement the ctor in _constructFilteredCommand
        _constructFilteredCommand(item);
    }

    // We need to actually implement the ctor in a separate helper. This is
    // because we have a FilteredTask class which derives from FilteredCommand.
    // HOWEVER, for cppwinrt ~ r e a s o n s ~, it doesn't actually derive from
    // FilteredCommand directly, so we can't just use the FilteredCommand ctor
    // directly in the base class.
    void FilteredCommand::_constructFilteredCommand(const winrt::TerminalApp::IPaletteItem& item)
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

    static std::tuple<std::vector<winrt::TerminalApp::HighlightedRun>, int32_t> _matchedSegmentsAndWeight(const std::shared_ptr<fzf::matcher::Pattern>& pattern, const winrt::hstring& haystack)
    {
        std::vector<winrt::TerminalApp::HighlightedRun> segments;
        int32_t weight = 0;

        if (pattern && !pattern->terms.empty())
        {
            if (auto match = fzf::matcher::Match(haystack, *pattern.get()); match)
            {
                auto& matchResult = *match;
                weight = matchResult.Score;
                segments.resize(matchResult.Runs.size());
                std::transform(matchResult.Runs.begin(), matchResult.Runs.end(), segments.begin(), [](auto&& run) -> winrt::TerminalApp::HighlightedRun {
                    return { run.Start, run.End };
                });
            }
        }
        return { std::move(segments), weight };
    }

    void FilteredCommand::_update()
    {
        auto [segments, weight] = _matchedSegmentsAndWeight(_pattern, _Item.Name());

        if (segments.empty())
        {
            NameHighlights(nullptr);
        }
        else
        {
            NameHighlights(winrt::single_threaded_vector(std::move(segments)));
        }

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
