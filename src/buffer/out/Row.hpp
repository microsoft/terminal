/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Row.hpp

Abstract:
- data structure for information associated with one row of screen buffer

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "AttrRow.hpp"
#include "LineRendition.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"
#include "unicode.hpp"

// For use when til::rle supports .replace
#define TIL_RLE_WORKS_LIKE_STRING 1
// When 0, use basic_string<uint16_t> for column counts
#define ROW_USE_RLE 1
#define BACKING_BUFFER_IS_STRINGLIKE (!ROW_USE_RLE || TIL_RLE_WORKS_LIKE_STRING)

#if ROW_USE_RLE
#include <til/rle.h>
#endif

#pragma warning(push)
#pragma warning(disable : 4267)
class TextBuffer;

enum class DelimiterClass
{
    ControlChar,
    DelimiterChar,
    RegularChar
};

class ROW final
{
public:
    ROW(const SHORT rowId, const unsigned short rowWidth, const TextAttribute fillAttribute, TextBuffer* const pParent);

    size_t size() const noexcept { return _rowWidth; }

    void SetWrapForced(const bool wrap) noexcept { _wrapForced = wrap; }
    bool WasWrapForced() const noexcept { return _wrapForced; }

    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept { _doubleBytePadded = doubleBytePadded; }
    bool WasDoubleBytePadded() const noexcept { return _doubleBytePadded; }

    const ATTR_ROW& GetAttrRow() const noexcept { return _attrRow; }
    ATTR_ROW& GetAttrRow() noexcept { return _attrRow; }

    LineRendition GetLineRendition() const noexcept { return _lineRendition; }
    void SetLineRendition(const LineRendition lineRendition) noexcept { _lineRendition = lineRendition; }

    bool Reset(const TextAttribute Attr);
    [[nodiscard]] HRESULT Resize(const unsigned short width);

    void ClearColumn(const size_t column);
    std::wstring GetText() const { return _data; }

    OutputCellIterator WriteCells(OutputCellIterator it, const size_t index, const std::optional<bool> wrap = std::nullopt, std::optional<size_t> limitRight = std::nullopt);

#ifdef UNIT_TESTING
    friend constexpr bool operator==(const ROW& a, const ROW& b) noexcept;
    friend class RowTests;
#endif

private:
    ATTR_ROW _attrRow;
    LineRendition _lineRendition;
    unsigned short _rowWidth;
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced;
    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded;

public:
    std::wstring _data;
#if !ROW_USE_RLE
    std::basic_string<uint16_t> _cwid;
#else
    til::rle<uint8_t, uint16_t> _cwid;
#endif

    std::tuple<size_t, size_t, size_t, size_t> _indicesForCol(size_t col, typename decltype(_data)::size_type hint = 0, size_t colstart = 0) const
    {
#if 1
        size_t currentCol{ 0 };
        size_t currentWchar{ 0 };
        auto it{ _cwid.runs().cbegin() };
        while (it != _cwid.runs().cend())
        {
            // Each compressed pair tells us how many columns x N wchar_t
            const auto colsCoveredByRun{ it->value * it->length };
            if (currentCol + colsCoveredByRun > col)
            {
                // We want to break out of the loop to manually handle this run, because
                // we've determined that it is the run that covers the column of interest.
                break;
            }
            currentCol += colsCoveredByRun;
            currentWchar += it->length;
            it++;
        }

        if (it == _cwid.runs().cend())
        {
            // SHOULD NEVER HIT THIS???
            __debugbreak();
        }
        // currentWchar is how many wchar_t we are into the string before processing this run
        // currentCol is how many columns we've covered before processing this run

        // We are *guaranteed* that the hit is in this run -- no need to check it->length
        // col-currentCol is how many columns are left unaccounted for (how far into this run we need to go)
        const auto colsLeftToCountInCurrentRun{ col - currentCol };
        currentWchar += colsLeftToCountInCurrentRun / it->value; // one wch per column unit -- rounds down (correct behavior)

        size_t lenInWchars{ 1 }; // the first hit takes up one wchar

        // We use this to determine if we have exhausted every column this run can cough up.
        // colsLeftToCountInCurrentRun is 0-indexed, but colsConsumedByRun is 1-indexed (index 0 consumes N columns, etc.)
        // therefore, we reindex colsLeftToCountInCurrentRun and compare it to colsConsumedByRun
        const auto colsConsumedFromRun{ colsLeftToCountInCurrentRun + it->value };
        const auto colsCoveredByRun{ it->value * it->length };
        // If we *have* consumed every column this run can provide, we must check the run after it:
        // if it contributes "0" columns, it is actually a set of trailing code units.
        if (colsConsumedFromRun >= colsCoveredByRun && it != _cwid.runs().cend())
        {
            const auto nextRunIt{ it + 1 };
            if (nextRunIt != _cwid.runs().cend() && nextRunIt->value == 0)
            {
                // we were at the boundary of a column run, so if the next one is 0 it tells us that each
                // wchar after it is a trailer
                lenInWchars += nextRunIt->length;
            }
        }

        return {
            currentWchar, // wchar start
            lenInWchars, // wchar size
            colsLeftToCountInCurrentRun % it->value, // how far into the value's column count we were (if we are partway through a 2-wide or 3-wide glyph)
            it->value // how many columns is the thing we hit?
        };
#endif

#if 0
        // WISHLIST: I want to be able to iterate *compressed runs* so that I don't need to sum up individual column counts one by one
        size_t c{ colstart };
        auto it{ _cwid.cbegin() + hint };
        while (it != _cwid.cend())
        {
            if (c + *it > col)
            {
                // if we would overshoot, be done
                break;
            }
            c += *it++;
        } // accumulate columns until we hit the requested one
        if (it == _cwid.cend())
        {
            // we never hit it!
            throw 0;
        }
        // it points at the column width of the character that tripped us over the limit (char that was a hit)
        auto eit{ std::find_if_not(it + 1, _cwid.cend(), [](auto&& c) { return c == 0; }) };
        // eit points at first column that is nonzero after it, or the end
        const auto len{ eit - it };
        const auto cols{ *it }; // how big was the char we landed on
        return { it - _cwid.cbegin(), len, col - c, cols };
#endif
    }

public:
    std::wstring_view GlyphAt(size_t col) const
    {
        auto [begin, len, off, _] = _indicesForCol(col);
        return { _data.data() + begin, len };
    }

    std::pair<size_t, size_t> WriteGlyphAtMeasured(size_t col, size_t ncols, std::wstring_view glyph, typename decltype(_data)::size_type hint = 0, size_t colstart = 0)
    {
        // When we want to replace a column, or set of columns, with a glyph, we need to:
        // * Figure out the physical extent of the character in that cell (UTF-16 code units).
        // * Figure out the columnar extent of the character in that cell (how many columns it covers).
        //  * In the simple case (1->1, 2->2), there will be no damage.
        //  * In the complex case (2->1, 1->2, 2->2 with middle overlap), there *WILL* be damage.
        // * Replace the physical character data in that cell with the new character data.
        // * Insert padding characters to the left and right to account for damage.
        //
        // ## DAMAGE
        // Damage is measured in the number of columns to the left
        // and right of the new glyph that are now NO LONGER VALID because
        // they were double-width characters that are being cut in half,
        // or single-width characters that are collateral damage from stomping
        // them with a double-width character.
        auto [begin, len, off, cols]{ _indicesForCol(col, hint, colstart) };
        const auto minDamageColumn{ col - off }; // Column damage to the left (where we overlapped the right of a wide glyph)
        auto maxDamageColumnExclusive{ minDamageColumn + cols }; // Column damage to the right (where we overlapped the left of a wide glyph)

        while (maxDamageColumnExclusive < col + ncols)
        {
            auto [nbegin, nlen, noff, newcols]{ _indicesForCol(maxDamageColumnExclusive, begin, minDamageColumn) };
            // *INVARIANT* the beginning of the next column range must have a different beginning byte
            // This column began at a different data index, so we have to delete its data too.
            // Since it's contiguous, just increment len.
            len += nlen;
            maxDamageColumnExclusive += newcols;
        }

        if (minDamageColumn == col && maxDamageColumnExclusive == col + ncols)
        {
            // We are only damaging as many columns as we are introducing -- no spillover (!)
            // We can replace the code units in the data directly, and we can replace the
            // column counts with [col, 0, 0...] (with as many zeroes as we need to account
            // for any code units past the first.)
            _data.replace(begin, len, glyph);
#if BACKING_BUFFER_IS_STRINGLIKE // rle doesn't work like string here
            typename decltype(_cwid)::rle_type newRuns[]{
                { gsl::narrow_cast<uint8_t>(ncols), 1 },
                { 0, gsl::narrow_cast<uint16_t>(glyph.size() - 1) },
            };
            _cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), gsl::make_span(&newRuns[0], glyph.size() == 1 ? 1 : 2));
#else
            auto old = _cwid.substr(uint16_t(begin + len));
            _cwid.fill(ncols, begin);
            _cwid.fill(0, begin + 1);
            _cwid.assign(old.run_cbegin(), old.run_cend(), begin + glyph.size());
            _cwid.resize(_data.size());
#endif
        }
        else
        {
            // We are damaging multiple columns -- oops. We need to insert replacement characters
            // to get us from the leftmost side of the damaged glyph up to the leftmost side of
            // our newly-inserted region. We also need to insert replacement characters from the
            // rightmost side of our glyph to the rightmost side of the glyph that was once in
            // that column.
            // Left side count : col - minDamageColumn
            // Right side count: maxDamageColumn - (col + ncols)
            const auto replacementCodeUnits{ (col - minDamageColumn) + glyph.size() + (maxDamageColumnExclusive - (col + ncols)) };
            std::wstring replacement(replacementCodeUnits, UNICODE_SPACE);
            replacement.replace(col - minDamageColumn, glyph.size(), glyph);

            // New advances:
            //             Our glyph and all its trailers
            //             v-----v
            // [1, ..., 1, X, 0, 0, 1, ..., 1]
            //  ^-------^           ^-------^
            //  Each replacement space char
            //  is one column wide. We have
            //  to insert [1]s for each
            //  damaged column.
#if BACKING_BUFFER_IS_STRINGLIKE // rle doesn't work like string here
            boost::container::small_vector<typename decltype(_cwid)::rle_type, 4> newRuns;
            if (col - minDamageColumn)
            {
                newRuns.emplace_back((uint8_t)1, gsl::narrow_cast<uint16_t>(col - minDamageColumn));
            }
            newRuns.emplace_back(gsl::narrow_cast<uint8_t>(ncols), (uint16_t)1);
            if (glyph.size() > 1)
            {
                newRuns.emplace_back((uint8_t)0, gsl::narrow_cast<uint16_t>(glyph.size() - 1)); // trailers
            }
            if (maxDamageColumnExclusive - (col + ncols))
            {
                newRuns.emplace_back((uint8_t)1, gsl::narrow_cast<uint16_t>(maxDamageColumnExclusive - (col + ncols)));
            }
            _data.replace(begin, len, replacement);
            _cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), gsl::make_span(newRuns));
#else
            auto old = _cwid.substr(uint16_t(begin + replacementCodeUnits));
            _data.replace(begin, len, replacement);
            _cwid.resize(_data.size());
            _cwid.fill(1, begin);
            _cwid.fill(ncols, begin + (col - minDamageColumn));
            _cwid.fill(0, begin + (col - minDamageColumn) + 1);
            _cwid.fill(1, begin + (col - minDamageColumn) + glyph.size());
            _cwid.assign(old.run_cbegin(), old.run_cend(), begin + replacementCodeUnits);
            _cwid.resize(_data.size());
#endif
        }
        if (_cwid.size() != _data.size())
        {
#if BACKING_BUFFER_IS_STRINGLIKE // rle doesn't work like string
            _cwid.resize_trailing_extent(gsl::narrow_cast<uint16_t>(_data.size()));
#else
            const auto old{ _cwid.size() };
            _cwid.resize(_data.size());
            if (old < _cwid.size())
                _cwid.fill(0, old);
#endif
        }

        // Distance from requested column to final
        _maxc = std::max(_maxc, maxDamageColumnExclusive);
        return { begin + glyph.size(), col + ncols };
    }

    DbcsAttribute DbcsAttrAt(size_t col) const
    {
        auto [begin, len, off, ncols] = _indicesForCol(col);
        if (ncols == 1)
        {
            return DbcsAttribute{ DbcsAttribute::Attribute::Single };
        }
        else if (off >= 1)
        {
            return DbcsAttribute{ DbcsAttribute::Attribute::Trailing };
        }
        else if (off == 0)
        {
            return DbcsAttribute{ DbcsAttribute::Attribute::Leading };
        }
        return DbcsAttribute{ DbcsAttribute::Attribute::Single };
    }

    // Method Description:
    // - get delimiter class for a position in the char row
    // - used for double click selection and uia word navigation
    // Arguments:
    // - column: column to get text data for
    // - wordDelimiters: the delimiters defined as a part of the DelimiterClass::DelimiterChar
    // Return Value:
    // - the delimiter class for the given char
    const DelimiterClass DelimiterClassAt(const size_t column, const std::wstring_view wordDelimiters) const
    {
        THROW_HR_IF(E_INVALIDARG, column >= _rowWidth);

        const auto glyph = *GlyphAt(column).begin();
        if (glyph <= UNICODE_SPACE)
        {
            return DelimiterClass::ControlChar;
        }
        else if (wordDelimiters.find(glyph) != std::wstring_view::npos)
        {
            return DelimiterClass::DelimiterChar;
        }
        else
        {
            return DelimiterClass::RegularChar;
        }
    }

    size_t _maxc{};
    size_t MeasureRight() const
    {
        return _maxc;
    }
};

#ifdef UNIT_TESTING
constexpr bool operator==(const ROW& a, const ROW& b) noexcept
{
    // comparison is only used in the tests; this should suffice.
    return (a._data == b._data &&
            a._cwid == b._cwid &&
            a._attrRow == b._attrRow &&
            a._rowWidth == b._rowWidth &&
            a._wrapForced == b._wrapForced &&
            a._doubleBytePadded == b._doubleBytePadded);
}
#endif
#pragma warning(pop)
