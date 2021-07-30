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
#include "unicode.hpp"

#pragma warning(push)
#pragma warning(disable : 4267)
class TextBuffer;

using RowMeasurementBuffer = til::small_rle<uint8_t, uint16_t, 3>;

struct DamageRanges
{
    size_t dataOffset;
    size_t dataLength;
    uint16_t firstColumn;
    uint16_t lastColumnExclusive;
};

template<typename TRuns>
static DamageRanges DamageRangesForColumnInMeasurementBuffer(const TRuns& cwid, size_t col)
{
    size_t currentCol{ 0 };
    size_t currentWchar{ 0 };
    auto it{ cwid.runs().cbegin() };
    while (it != cwid.runs().cend())
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

    if (it == cwid.runs().cend())
    {
        // this is an interesting case- somebody requested a column we cannot answer for.
        // The string might actually have data, and the caller might be interested in where that data is.
        // Ideally, we would return the index of the first char out-of-bounds, and the length of the remaining data as a single unit.
        // We can't answer for how much space it takes up, though.
        __debugbreak();
        return { 0, 0, 0u, 0u };
        //return { currentWchar, _data.size() - currentWchar, 0u, 0u };
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
    if (colsConsumedFromRun >= colsCoveredByRun && it != cwid.runs().cend())
    {
        const auto nextRunIt{ it + 1 };
        if (nextRunIt != cwid.runs().cend() && nextRunIt->value == 0)
        {
            // we were at the boundary of a column run, so if the next one is 0 it tells us that each
            // wchar after it is a trailer
            lenInWchars += nextRunIt->length;
        }
    }

    return {
        currentWchar, // wchar start
        lenInWchars, // wchar size
        gsl::narrow_cast<uint16_t>(col - (colsLeftToCountInCurrentRun % it->value)), // Column damage to the left (where we overlapped the right of a wide glyph)
        gsl::narrow_cast<uint16_t>(col - (colsLeftToCountInCurrentRun % it->value) + it->value), // Column damage to the right (where we overlapped the left of a wide glyph)
    };
}

class ROW;
struct RowImage
{
    std::wstring _data;
    RowMeasurementBuffer _cwid;
    ATTR_ROW _attrRow;
    uint16_t _width;
    friend class ROW;
    friend class TextBuffer;
    RowImage() :
        _data{}, _cwid{}, _attrRow{ {} }, _width{ 0 } {}
    RowImage(const std::wstring& data, const RowMeasurementBuffer& cwid, ATTR_ROW attrRow, uint16_t width):
        _data{data}, _cwid{cwid}, _attrRow{std::move(attrRow)}, _width{width} {}

    // exclusive
    std::tuple<RowImage, RowImage> split(uint16_t col) const
    {
        if (col >= _width)
        {
            return { *this, RowImage{} };
        }
        else if (col == 0)
        {
            return { RowImage{}, *this };
        }
        auto yes_more_fucking_damage_ranges = DamageRangesForColumnInMeasurementBuffer(_cwid, col);
        // here's a dumb decision: when you split along a wide char, you get spaces over its damage on the left side.
        // X X XX X X X
        //     ^ split
        // X X S        <- left
        //     XX X X X <- right
        auto chopped_off = col == yes_more_fucking_damage_ranges.lastColumnExclusive ? 0 : /*didn't get whole thing*/ yes_more_fucking_damage_ranges.lastColumnExclusive - col;
        auto chopped_on = col == yes_more_fucking_damage_ranges.lastColumnExclusive ? yes_more_fucking_damage_ranges.dataLength : 0;
        auto lwid = _cwid.slice(0, yes_more_fucking_damage_ranges.dataOffset + chopped_on);
        for (auto z{ chopped_off }; z > 0; --z)
        {
            lwid.append(1);
        }
        RowImage left{
            _data.substr(0, yes_more_fucking_damage_ranges.dataOffset + chopped_on) + std::wstring(chopped_off, UNICODE_SPACE),
            lwid,
            _attrRow._data.slice(0, yes_more_fucking_damage_ranges.lastColumnExclusive),
            col
        };
        RowImage right{
            _data.substr(yes_more_fucking_damage_ranges.dataOffset + chopped_on),
            _cwid.slice(yes_more_fucking_damage_ranges.dataOffset + chopped_on, _data.length()),
            // we use first col here to catch the overlap
            _attrRow._data.slice(yes_more_fucking_damage_ranges.firstColumn, _width),
            ::base::ClampSub(_width, col),
        };
        return { left, right };
    }
};

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

    size_t size() const noexcept { return _data._width; }

    void SetWrapForced(const bool wrap) noexcept { _wrapForced = wrap; }
    bool WasWrapForced() const noexcept { return _wrapForced; }

    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept { _doubleBytePadded = doubleBytePadded; }
    bool WasDoubleBytePadded() const noexcept { return _doubleBytePadded; }

    const ATTR_ROW& GetAttrRow() const noexcept { return _data._attrRow; }
    ATTR_ROW& GetAttrRow() noexcept { return _data._attrRow; }

    LineRendition GetLineRendition() const noexcept { return _lineRendition; }
    void SetLineRendition(const LineRendition lineRendition) noexcept { _lineRendition = lineRendition; }

    bool Reset(const TextAttribute Attr);
    [[nodiscard]] HRESULT Resize(const unsigned short width);

    void ClearColumn(const size_t column);
    std::wstring GetText() const { return _data._data; }

#ifdef UNIT_TESTING
    friend constexpr bool operator==(const ROW& a, const ROW& b) noexcept;
    friend class RowTests;
#endif

    struct RowData
    {
        std::wstring _data;
        RowMeasurementBuffer _cwid;
        ATTR_ROW _attrRow;
        unsigned short _width;

        DamageRanges _damageForColumn(size_t col) const
        {
            return DamageRangesForColumnInMeasurementBuffer(_cwid, col);
        }

        DamageRanges _damageForColumns(size_t col, size_t ncols) const
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
            auto damage{ _damageForColumn(col) };
            const auto lastDamage{ _damageForColumn(col + ncols - 1 /*inclusive*/) };

            // *INVARIANT* the beginning of the next column range must have a different beginning byte
            // This column began at a different data index, so we have to delete its data too.
            // Since it's contiguous, just increment len.
            damage.dataLength = lastDamage.dataOffset + lastDamage.dataLength - damage.dataOffset;
            damage.lastColumnExclusive = lastDamage.lastColumnExclusive;

            return damage;
        }

        void _strikeDamageAndAdjust(size_t col, size_t ncols, size_t incomingCodeUnitCount, DamageRanges& range)
        {
            (void)incomingCodeUnitCount;
            const bool damaged{ range.firstColumn < col || col + ncols < range.lastColumnExclusive };
            if (damaged)
            {
                const auto damagedColumns{ range.lastColumnExclusive - range.firstColumn };
                _data.replace(range.dataOffset, range.dataLength, damagedColumns, UNICODE_SPACE);
                _cwid.replace(range.dataOffset, range.dataOffset + range.dataLength, { uint8_t{ 1 }, gsl::narrow_cast<uint16_t>(damagedColumns) });
                // We may have replaced surrogate pairs/etc with fewer/more code units.
                range.dataLength = damagedColumns;
            }
        }
    };

private:
    RowData _data;
    LineRendition _lineRendition;
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced;
    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded;

public:
    std::wstring_view GlyphAt(size_t col) const
    {
        const auto lookup{ _data._damageForColumn(col) };
        return { _data._data.data() + lookup.dataOffset, lookup.dataLength };
    }

    std::pair<size_t, size_t> WriteGlyphAtMeasured(size_t col, size_t ncols, std::wstring_view glyph)
    {
        const auto [begin, len, minDamageColumn, maxDamageColumnExclusive]{ _data._damageForColumns(col, ncols) };

        if (minDamageColumn == col && maxDamageColumnExclusive == col + ncols)
        {
            // We are only damaging as many columns as we are introducing -- no spillover (!)
            // We can replace the code units in the data directly, and we can replace the
            // column counts with [col, 0, 0...] (with as many zeroes as we need to account
            // for any code units past the first.)
            _data._data.replace(begin, len, glyph);
            typename decltype(_data._cwid)::rle_type newRuns[]{
                { gsl::narrow_cast<uint8_t>(ncols), 1 },
                { 0, gsl::narrow_cast<uint16_t>(glyph.size() - 1) },
            };
            _data._cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), gsl::make_span(&newRuns[0], glyph.size() == 1 ? 1 : 2));
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
            boost::container::small_vector<typename decltype(_data._cwid)::rle_type, 4> newRuns;
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
            _data._data.replace(begin, len, replacement);
            _data._cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), gsl::make_span(newRuns));
        }

        // Distance from requested column to final
        _maxc = std::max(_maxc, maxDamageColumnExclusive);
        return { begin + glyph.size(), col + ncols };
    }

    DbcsAttribute DbcsAttrAt(size_t col) const
    {
        const auto [begin, len, first, lastE] = _data._damageForColumn(col);
        if (lastE - first == 1)
        {
            // The glyph under this column is only onw column wide.
            return DbcsAttribute{ DbcsAttribute::Attribute::Single };
        }
        else if (first != col)
        {
            // The glyph under this column is >1 col wide, and we're bisecting it
            return DbcsAttribute{ DbcsAttribute::Attribute::Trailing };
        }
        else
        {
            // The glyph under this column is >1 col wide, and we're at the head
            return DbcsAttribute{ DbcsAttribute::Attribute::Leading };
        }
    }

    std::tuple<size_t, uint16_t, uint16_t> WriteStringAtMeasured(uint16_t col, uint16_t colCount, const std::wstring_view& string, const RowMeasurementBuffer& measurements)
    {
        size_t incomingLastColumn{ std::min<size_t>(_data._width - col, colCount) };
        auto incomingLastColumnOffsets{ DamageRangesForColumnInMeasurementBuffer(measurements, incomingLastColumn - 1 /*inclusive*/) };

        auto codeUnitsToConsume{ incomingLastColumnOffsets.dataOffset };
        auto columnsToConsume{ incomingLastColumn };

        const auto [begin, len, minDamageColumn, maxDamageColumnExclusive]{ _data._damageForColumns(col, incomingLastColumn) };

        // If these don't match, we are cutting a multi-cell glyph.
        if (incomingLastColumnOffsets.lastColumnExclusive == incomingLastColumn)
        {
            // Since they *do* match, we should consume this part of the string too.
            codeUnitsToConsume += incomingLastColumnOffsets.dataLength;
        }
        else
        {
            // Only consume up to the final cell (the one we cut in half)
            columnsToConsume = incomingLastColumnOffsets.firstColumn;
            // **INVARIANT** we only get here if we had to cut off the incoming text, and that only
            // happens because we had to clamp the read buffer against our width. This means that the
            // incoming text definitely had a wide glyph that would not fit against the end of our
            // buffer.
            // THEREFORE: col+incomingLastColumn was our final column (exclusive)
            // which means that maxDamageColumnExclusive can be upgraded to be our width.
            // OPTIMIZATION: If we mark our last column as damaged, it will automatically get stomped
            // with spaces.
            // NO NO NO NO NO NO NO NO NO NO TODO(DH)
            // We can't do this without changing the damaged buffer region to delete the character
            // from _data at the same time. Use the non-optimial path.
            ClearColumn(_data._width - 1);
            SetDoubleBytePadded(true);
            // NO - we already marked this column when we calculated damage above
            //++columnsWrittenAfterInsertionPoint;
        }

        auto mss = measurements.slice(0, gsl::narrow_cast<uint16_t>(codeUnitsToConsume));
        if (minDamageColumn == col && maxDamageColumnExclusive == col + incomingLastColumn)
        {
            // We are only damaging as many columns as we are introducing -- no spillover (!)
            // We can replace the code units in the data directly, and we can replace the
            // column counts with [col, 0, 0...] (with as many zeroes as we need to account
            // for any code units past the first.)
            _data._data.replace(begin, len, &*string.cbegin(), codeUnitsToConsume);
            _data._cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), mss.runs());
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
            const auto replacementCodeUnits{ (col - minDamageColumn) + codeUnitsToConsume + (maxDamageColumnExclusive - (col + incomingLastColumn)) };
            std::wstring replacement(replacementCodeUnits, UNICODE_SPACE);
            replacement.replace(col - minDamageColumn, codeUnitsToConsume, &*string.cbegin(), codeUnitsToConsume);

            // New advances:
            //             Our glyph and all its trailers
            //             v-----v
            // [1, ..., 1, X, 0, 0, 1, ..., 1]
            //  ^-------^           ^-------^
            //  Each replacement space char
            //  is one column wide. We have
            //  to insert [1]s for each
            //  damaged column.
            mss.replace(0, 0, { uint8_t{ 1 }, gsl::narrow_cast<uint16_t>(col - minDamageColumn) });
            mss.replace(mss.size(), mss.size(), { uint8_t{ 1 }, gsl::narrow_cast<uint16_t>(maxDamageColumnExclusive - (col + incomingLastColumn)) });
            _data._data.replace(begin, len, replacement);
            _data._cwid.replace(gsl::narrow_cast<uint16_t>(begin), gsl::narrow_cast<uint16_t>(begin + len), mss.runs()); //gsl::make_span(newRuns));
        }

        return {
            codeUnitsToConsume,
            gsl::narrow_cast<uint16_t>(maxDamageColumnExclusive - col),
            gsl::narrow_cast<uint16_t>(columnsToConsume)
        };
    }

    size_t Fill(size_t col, size_t count, wchar_t ch, uint8_t w)
    {
        const auto charsFitOrRemain = std::min((_data._width - col) / w, count);

        const auto columnsRequired{ charsFitOrRemain * w };
        auto damage = _data._damageForColumns(col, columnsRequired);
        // If we are filling over the left/right halves of a character
        // This is a bit wasteful since it can grow/shrink the buffers and we're about
        // to do it again, but I was trying to be expedient.
        _data._strikeDamageAndAdjust(col, columnsRequired, charsFitOrRemain, damage);

        const auto [begin, len, min, max] = damage;
        _data._data.replace(begin, len, charsFitOrRemain, ch);
        _data._cwid.replace(begin, begin + len, { w, gsl::narrow_cast<uint16_t>(charsFitOrRemain) });
        const auto doubleBytePadded{
            w > 1 // We had a wide glyph...
            && max != _data._width // ...and didn't reach the edge
            && count > charsFitOrRemain // ...but we had spare characters, so we wanted to
        };
        if (doubleBytePadded)
        {
            const uint16_t remaining{ gsl::narrow_cast<uint16_t>(_data._width - max) };
            // overflow: add spaces
            _data._data.replace(begin + charsFitOrRemain, _data._data.size() - begin + charsFitOrRemain, remaining, UNICODE_SPACE);
            _data._cwid.replace(begin + charsFitOrRemain, _data._cwid.size(), { uint8_t{ 1u }, gsl::narrow_cast<uint16_t>(remaining) });
        }
        if (max == _data._width || doubleBytePadded)
        {
            // TODO(DH): Evaluate the above condition
            // We only want to do this if we touched or near-touched the lat col
            SetDoubleBytePadded(doubleBytePadded);
            SetWrapForced(false);
        }
        return charsFitOrRemain;
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
        THROW_HR_IF(E_INVALIDARG, column >= _data._width);

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

    RowImage Dump(uint16_t left, uint16_t size)
    {
        auto [begin, len, min, max] = _data._damageForColumns(left, size);
        return RowImage{
            _data._data.substr(begin, len),
            _data._cwid.slice(begin, begin + len),
            ATTR_ROW{ _data._attrRow._data.slice(min, max) },
            ::base::MakeClampedNum(max) - min
        };
    }

    void Reinsert(uint16_t left, const RowImage& ri)
    {
        auto damage{ _data._damageForColumns(left, ri._width) };
        _data._strikeDamageAndAdjust(left, ri._width, ri._data.size(), damage);
        _data._data.replace(damage.dataOffset, damage.dataLength, ri._data);
        _data._cwid.replace(damage.dataOffset, damage.dataOffset + damage.dataLength, ri._cwid.runs());
        _data._attrRow._data.replace(damage.firstColumn, damage.lastColumnExclusive, ri._attrRow._data.runs());
    }

    uint16_t _maxc{};
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
