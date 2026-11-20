// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <string_view>
#include <til/string.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::CodePageUtils
{
    constexpr std::wstring_view UnsetTextForProfile(const bool isBaseLayer,
                                                    const std::wstring_view defaultText,
                                                    const std::wstring_view inheritedText) noexcept
    {
        return isBaseLayer ? defaultText : inheritedText;
    }

    inline bool MatchesFilter(const std::wstring_view displayName, const std::wstring_view filter) noexcept
    {
        return filter.empty() || til::contains_linguistic_insensitive(displayName, filter);
    }

    constexpr bool ShouldClearFromTextChange(const bool isUserInput, const bool isEmpty) noexcept
    {
        return isUserInput && isEmpty;
    }

    constexpr bool ShouldCommitSingleResult(const bool hasChosenSuggestion, const size_t filteredCount) noexcept
    {
        return !hasChosenSuggestion && filteredCount == 1;
    }
}
