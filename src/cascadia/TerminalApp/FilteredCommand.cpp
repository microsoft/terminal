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
        _Filter = L"";
        _Weight = 0;
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

    void FilteredCommand::UpdateFilter(const winrt::hstring& filter)
    {
        // If the filter was not changed we want to prevent the re-computation of matching
        // that might result in triggering a notification event
        if (filter != _Filter)
        {
            Filter(filter);
            Weight(_computeWeight());
            HighlightedName(_computeHighlightedName());
        }
    }

    // Method Description:
    // - Looks up the filter characters within the item name.
    // Using the fzf algorithm to traceback from the maximum score to highlight the chars with the
    // optimal match. (Preference is given to word boundaries, consecutive chars and special characters
    // while penalties are given for gaps)
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
    // Return Value:
    // - The HighlightedText object initialized with the segments computed according to the algorithm above.
    winrt::TerminalApp::HighlightedText FilteredCommand::_computeHighlightedName()
    {
        const auto segments = winrt::single_threaded_observable_vector<winrt::TerminalApp::HighlightedTextSegment>();
        auto commandName = _Item.Name();

        if (Weight() == 0)
        {
            segments.Append(winrt::TerminalApp::HighlightedTextSegment(commandName, false));
            return winrt::make<HighlightedText>(segments);
        }

        auto pattern = fzf::matcher::ParsePattern(Filter());
        auto positions = fzf::matcher::GetPositions(commandName, pattern);
        // positions are returned is sorted pairs by search term. E.g. sp anta {5,4,11,10,9,8}
        // sorting these in ascending order so it is easier to build the text segments
        std::ranges::sort(positions, std::less<>());
        // a position can be matched in multiple terms, removed duplicates to simplify segments
        positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
        size_t lastPosition = 0;
        for (auto position : positions)
        {
            if (position > lastPosition)
            {
                hstring nonMatchSegment{ commandName.data() + lastPosition, static_cast<unsigned>(position - lastPosition) };
                segments.Append(winrt::TerminalApp::HighlightedTextSegment(nonMatchSegment, false));
            }

            hstring matchSegment{ commandName.data() + position, 1 };
            segments.Append(winrt::TerminalApp::HighlightedTextSegment(matchSegment, true));

            lastPosition = position + 1;
        }

        if (lastPosition < commandName.size())
        {
            hstring segment{ commandName.data() + lastPosition, static_cast<unsigned>(commandName.size() - lastPosition) };
            segments.Append(winrt::TerminalApp::HighlightedTextSegment(segment, false));
        }

        return winrt::make<HighlightedText>(segments);
    }

    // Function Description:
    // - Calculates a "weighting" by which should be used to order a item
    //   name relative to other names, given a specific search string.
    //   Currently, this uses a derivative of the fzf implementation of the Smith Waterman algorithm:
    // - This will return 0 if the item should not be shown.
    //
    // Factors that affect a score (Taken from the fzf repository)
    // Scoring criteria
    // ----------------
    // 
    // - We prefer matches at special positions, such as the start of a word, or
    //   uppercase character in camelCase words.
    //   - Note everything is converted to lower case so this does not apply
    // 
    // - That is, we prefer an occurrence of the pattern with more characters
    //   matching at special positions, even if the total match length is longer.
    //     e.g. "fuzzyfinder" vs. "fuzzy-finder" on "ff"
    //                             ````````````
    // - Also, if the first character in the pattern appears at one of the special
    //   positions, the bonus point for the position is multiplied by a constant
    //   as it is extremely likely that the first character in the typed pattern
    //   has more significance than the rest.
    //     e.g. "fo-bar" vs. "foob-r" on "br"
    //           ``````
    // - But since fzf is still a fuzzy finder, not an acronym finder, we should also
    //   consider the total length of the matched substring. This is why we have the
    //   gap penalty. The gap penalty increases as the length of the gap (distance
    //   between the matching characters) increases, so the effect of the bonus is
    //   eventually cancelled at some point.
    //     e.g. "fuzzyfinder" vs. "fuzzy-blurry-finder" on "ff"
    //           ```````````
    // - Consequently, it is crucial to find the right balance between the bonus
    //   and the gap penalty. The parameters were chosen that the bonus is cancelled
    //   when the gap size increases beyond 8 characters.
    // 
    // - The bonus mechanism can have the undesirable side effect where consecutive
    //   matches are ranked lower than the ones with gaps.
    //     e.g. "foobar" vs. "foo-bar" on "foob"
    //                        ```````
    // - To correct this anomaly, we also give extra bonus point to each character
    //   in a consecutive matching chunk.
    //     e.g. "foobar" vs. "foo-bar" on "foob"
    //           ``````
    // - The amount of consecutive bonus is primarily determined by the bonus of the
    //   first character in the chunk.
    //     e.g. "foobar" vs. "out-of-bound" on "oob"
    //                        ````````````
    // Arguments:
    // - searchText: the string of text to search for in `name`
    // - name: the name to check
    // Return Value:
    // - the relative weight of this match
    int FilteredCommand::_computeWeight()
    {
        auto pattern = fzf::matcher::ParsePattern(Filter());
        auto score = fzf::matcher::GetScore(Item().Name(), pattern);
        return score;
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
            std::wstring_view firstName{ first.Item().Name() };
            std::wstring_view secondName{ second.Item().Name() };
            return lstrcmpi(firstName.data(), secondName.data()) < 0;
        }

        return firstWeight > secondWeight;
    }
}
