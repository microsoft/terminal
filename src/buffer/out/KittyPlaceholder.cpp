// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "KittyPlaceholder.hpp"

#include "KittyImages.hpp"
#include "Row.hpp"
#include "textBuffer.hpp"
#include "textBufferCellIterator.hpp"
#include "TextColor.h"

#include "KittyDiacritics.inc"

namespace
{
    // U+10EEEE is beyond the BMP, so in UTF-16 it is stored as a surrogate
    // pair. Cells hold grapheme clusters, so the pair lives in one cell.
    constexpr wchar_t SurrogateHigh = 0xDB3B;
    constexpr wchar_t SurrogateLow = 0xDEEE;

    struct Head
    {
        char32_t codepoint = 0;
        size_t length = 0; // UTF-16 units consumed
    };

    constexpr Head CodepointAt(const std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }
        const auto first = static_cast<char32_t>(text.front());
        if (first >= 0xD800 && first <= 0xDBFF && text.size() > 1)
        {
            const auto second = static_cast<char32_t>(text[1]);
            if (second >= 0xDC00 && second <= 0xDFFF)
            {
                return { static_cast<char32_t>(0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00)), 2 };
            }
        }
        return { first, 1 };
    }

    constexpr bool IsPlaceholderHead(const std::wstring_view text)
    {
        return text.size() >= 2 && text[0] == SurrogateHigh && text[1] == SurrogateLow;
    }

    int DiacriticValueImpl(const char32_t ch) noexcept
    {
        for (size_t i = 0; i < Diacritics.size(); ++i)
        {
            if (Diacritics[i] == ch)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}

namespace KittyPlaceholder
{
    std::optional<int> DiacriticValue(const char32_t ch) noexcept
    {
        const auto value = DiacriticValueImpl(ch);
        if (value < 0)
        {
            return std::nullopt;
        }
        return value;
    }

    bool DecodeRow(const TextBuffer& buffer, const ROW& row, const til::CoordType rowY, std::vector<Cell>& cells)
    {
        cells.clear();
        if (row.GetText().find(SurrogateHigh) == std::wstring_view::npos)
        {
            return false;
        }

        const auto rowWidth = row.size();
        auto it = buffer.GetCellDataAt({ 0, rowY });

        // The previously decoded placeholder cell and its colors, for the
        // diacritic inheritance rules.
        const Cell* previous = nullptr;
        TextColor previousForeground;
        TextColor previousUnderline;
        bool hasPrevious = false;

        for (til::CoordType column = 0; column < rowWidth; ++column, ++it)
        {
            const auto& view = *it;
            const auto text = view.Chars();
            if (!IsPlaceholderHead(text))
            {
                hasPrevious = false;
                continue;
            }

            const auto& attr = view.TextAttr();
            const auto& foreground = attr.GetForeground();
            const auto& underline = attr.GetUnderlineColor();

            Cell cell;
            cell.column = column;

            // Up to three diacritics: row, column, image id high byte.
            size_t i = 2;
            int rowValue = -1, columnInGrid = -1, imageIdHigh = -1;
            if (i < text.size())
            {
                const auto head = CodepointAt(text.substr(i));
                if (const auto value = DiacriticValueImpl(head.codepoint); value >= 0)
                {
                    rowValue = value;
                    i += head.length;
                }
            }
            if (i < text.size())
            {
                const auto head = CodepointAt(text.substr(i));
                if (const auto value = DiacriticValueImpl(head.codepoint); value >= 0)
                {
                    columnInGrid = value;
                    i += head.length;
                }
            }
            if (i < text.size())
            {
                const auto head = CodepointAt(text.substr(i));
                if (const auto value = DiacriticValueImpl(head.codepoint); value >= 0)
                {
                    imageIdHigh = value;
                    i += head.length;
                }
            }

            // Inheritance from the cell to the left, per the spec. A cell
            // continues a run only when the previous cell was a placeholder
            // with the same foreground and underline colors.
            if (hasPrevious && foreground == previousForeground && underline == previousUnderline)
            {
                if (rowValue < 0 && columnInGrid < 0 && imageIdHigh < 0)
                {
                    rowValue = previous->row;
                    columnInGrid = previous->columnInGrid + 1;
                    imageIdHigh = previous->imageIdHigh;
                }
                else if (rowValue >= 0 && columnInGrid < 0 && imageIdHigh < 0 && rowValue == previous->row)
                {
                    columnInGrid = previous->columnInGrid + 1;
                    imageIdHigh = previous->imageIdHigh;
                }
                else if (rowValue >= 0 && columnInGrid >= 0 && imageIdHigh < 0 && rowValue == previous->row && columnInGrid == previous->columnInGrid + 1)
                {
                    imageIdHigh = previous->imageIdHigh;
                }
            }

            cell.row = rowValue;
            cell.columnInGrid = columnInGrid;
            cell.imageIdHigh = imageIdHigh;

            if (foreground.IsIndex16() || foreground.IsIndex256())
            {
                cell.imageIdLow = foreground.GetIndex();
                cell.indexedColor = true;
            }
            else if (foreground.IsRgb())
            {
                // GetRGB returns a COLORREF (0x00BBGGRR); the client packs
                // the id as RRGGBB, so swap the byte order.
                const auto rgb = foreground.GetRGB();
                cell.imageIdLow = static_cast<int>(((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF));
            }

            cells.push_back(cell);
            previous = &cells.back();
            previousForeground = foreground;
            previousUnderline = underline;
            hasPrevious = true;
        }

        return !cells.empty();
    }

    std::wstring StripPlaceholders(const std::wstring_view text)
    {
        std::wstring result;
        auto position = text.find(SurrogateHigh);
        if (position == std::wstring_view::npos)
        {
            result.assign(text);
            return result;
        }

        result.reserve(text.size());
        result.append(text.substr(0, position));

        while (position != std::wstring_view::npos)
        {
            // Skip the surrogate pair and any trailing diacritics. A lone
            // trailing high surrogate leaves position past the end; clamp
            // it or the find below throws.
            position += 2;
            if (position > text.size())
            {
                position = text.size();
            }
            while (position < text.size())
            {
                const auto head = CodepointAt(text.substr(position));
                if (DiacriticValueImpl(head.codepoint) < 0)
                {
                    break;
                }
                position += head.length;
            }
            const auto next = text.find(SurrogateHigh, position);
            const auto end = next == std::wstring_view::npos ? text.size() : next;
            result.append(text.substr(position, end - position));
            position = next;
        }
        return result;
    }
}
