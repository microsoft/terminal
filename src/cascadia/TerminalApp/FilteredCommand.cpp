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
    FilteredCommand::FilteredCommand(const winrt::TerminalApp::IPaletteItem& item) :
        _Item{ item }, _Weight{ 0 }
    {
        // Recompute the highlighted name if the item name changes
        // Our Item will not change, so we don't need to update the revoker if it does.
        _itemChangedRevoker = _Item.as<winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>().PropertyChanged(winrt::auto_revoke, [=](auto& /*sender*/, auto& e) {
            const auto property{ e.PropertyName() };
            if (property == L"Name")
            {
                _update();
            }
            else if (property == L"Subtitle")
            {
                _update();
                PropertyChanged.raise(*this, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"HasSubtitle" });
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

    bool FilteredCommand::HasSubtitle()
    {
        return !_Item.Subtitle().empty();
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
        auto itemName = _Item.Name();
        auto [segments, weight] = _matchedSegmentsAndWeight(_pattern, itemName);
        decltype(segments) subtitleSegments;

        if (HasSubtitle())
        {
            auto itemSubtitle = _Item.Subtitle();
            int32_t subtitleWeight = 0;
            std::tie(subtitleSegments, subtitleWeight) = _matchedSegmentsAndWeight(_pattern, itemSubtitle);
            weight = std::max(weight, subtitleWeight);
        }

        if (segments.empty())
        {
            NameHighlights(nullptr);
        }
        else
        {
            NameHighlights(winrt::single_threaded_vector(std::move(segments)));
        }

        if (subtitleSegments.empty())
        {
            SubtitleHighlights(nullptr);
        }
        else
        {
            SubtitleHighlights(winrt::single_threaded_vector(std::move(subtitleSegments)));
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
