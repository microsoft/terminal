/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KittyPlaceholder.hpp

Abstract:
- Decoding of Kitty Graphics Protocol "Unicode placeholder" cells. A
  placement made this way is ordinary buffer text: the placeholder code
  point U+10EEEE followed by up to three combining diacritics encoding the
  cell's row, column and image-id high byte within the placement grid, with
  the low 24 (or 8) bits of the image id carried by the cell's foreground
  color. Because it is text it survives scrolling and reflow, and the
  renderer resolves it back to pixels at paint time.
--*/

#pragma once

#include "til.h"
#include <optional>
#include <string_view>

class ROW;
class TextBuffer;
class KittyImages;

namespace KittyPlaceholder
{
    constexpr char32_t Codepoint = 0x10EEEE;

    // The value (0-296) encoded by a row/column diacritic, or nullopt.
    std::optional<int> DiacriticValue(char32_t ch) noexcept;

    // One decoded placeholder cell.
    struct Cell
    {
        // Position of the base character within the row.
        til::CoordType column = 0;
        // Grid position encoded by the diacritics. Values that were absent
        // (and not inherited from the cell to the left) are -1.
        int row = -1;
        int columnInGrid = -1;
        int imageIdHigh = -1;
        // Image id low bits from the cell's foreground color. Index colors
        // contribute 8 bits, RGB colors 24 bits. -1 if the color is
        // neither (should not happen for well-formed placeholders).
        int imageIdLow = -1;
        // Whether the low bits came from an index color rather than RGB.
        bool indexedColor = false;
        // Placement id from the cell underline color, when it encodes one;
        // -1 otherwise. Selects among an image named virtual placements.
        int placementId = -1;
    };

    // Decodes the placeholder cells of a row into `cells`. Returns true if
    // the row contains at least one placeholder cell. The diacritic
    // inheritance rules from the spec are applied, so cells may omit all
    // diacritics and continue the run to their left.
    bool DecodeRow(const TextBuffer& buffer, const ROW& row, til::CoordType rowY, std::vector<Cell>& cells);

    // Strips placeholder sequences (the base character and its trailing
    // diacritics) from text meant for plain-text consumers such as the
    // clipboard and UIA, so they do not see private-use code points.
    std::wstring StripPlaceholders(std::wstring_view text);
}
