/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Cluster.hpp

Abstract:
- This serves as a structure to represent a single glyph cluster drawn on the screen
- This is required to enable N wchar_ts to consume M columns in the display
- Historically, the console only supported 1 wchar_t = 1 column or 1 wchar_t = 2 columns.

Author(s):
- Michael Niksa (MiNiksa) 25-Mar-2019
--*/

#pragma once

#include <unicode.hpp>

namespace Microsoft::Console::Render
{
    class Cluster
    {
    public:
        constexpr Cluster() noexcept = default;
        constexpr Cluster(const std::wstring_view text, const til::CoordType columns) noexcept :
            _text{ text },
            _columns{ columns }
        {
        }

        // Provides the embedded text as a single character
        // This might replace the string with the replacement character if it doesn't fit as one wchar_t.
        constexpr wchar_t GetTextAsSingle() const noexcept
        {
            if (_text.size() == 1)
            {
                return til::at(_text, 0);
            }
            else
            {
                return UNICODE_REPLACEMENT;
            }
        }

        // Provides the string of wchar_ts for this cluster.
        constexpr std::wstring_view GetText() const noexcept
        {
            return _text;
        }

        // Gets the number of columns in the grid that this character should consume
        // visually when rendered onto a line.
        constexpr til::CoordType GetColumns() const noexcept
        {
            return _columns;
        }

    private:
        // This is the UTF-16 string of characters that form a particular drawing cluster
        std::wstring_view _text;

        // This is how many columns we're expecting this cluster to take in the display grid
        til::CoordType _columns = 0;
    };
}
