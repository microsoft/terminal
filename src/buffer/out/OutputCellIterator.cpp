// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCellIterator.hpp"

#include "../../types/inc/convert.hpp"
#include "../../types/inc/Utf16Parser.hpp"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../inc/conattrs.hpp"

static constexpr TextAttribute InvalidTextAttribute{ INVALID_COLOR, INVALID_COLOR };

// Routine Description:
// - This is a fill-mode iterator for one particular wchar. It will repeat forever if fillLimit is 0.
// Arguments:
// - wch - The character to use for filling
// - fillLimit - How many times to allow this value to be viewed/filled. Infinite if 0.
OutputCellIterator::OutputCellIterator(const wchar_t& wch, const size_t fillLimit) noexcept :
    _mode(Mode::Fill),
    _currentView(s_GenerateView(wch)),
    _run(),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(fillLimit)
{
}

// Routine Description:
// - This is a fill-mode iterator for one particular color. It will repeat forever if fillLimit is 0.
// Arguments:
// - attr - The color attribute to use for filling
// - fillLimit - How many times to allow this value to be viewed/filled. Infinite if 0.
OutputCellIterator::OutputCellIterator(const TextAttribute& attr, const size_t fillLimit) noexcept :
    _mode(Mode::Fill),
    _currentView(s_GenerateView(attr)),
    _run(),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(fillLimit)
{
}

// Routine Description:
// - This is a fill-mode iterator for one particular character and color. It will repeat forever if fillLimit is 0.
// Arguments:
// - wch - The character to use for filling
// - attr - The color attribute to use for filling
// - fillLimit - How many times to allow this value to be viewed/filled. Infinite if 0.
OutputCellIterator::OutputCellIterator(const wchar_t& wch, const TextAttribute& attr, const size_t fillLimit) noexcept :
    _mode(Mode::Fill),
    _currentView(s_GenerateView(wch, attr)),
    _run(),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(fillLimit)
{
}

// Routine Description:
// - This is a fill-mode iterator for one particular CHAR_INFO. It will repeat forever if fillLimit is 0.
// Arguments:
// - charInfo - The legacy character and color data to use for fililng (uses Unicode portion of text data)
// - fillLimit - How many times to allow this value to be viewed/filled. Infinite if 0.
OutputCellIterator::OutputCellIterator(const CHAR_INFO& charInfo, const size_t fillLimit) noexcept :
    _mode(Mode::Fill),
    _currentView(s_GenerateView(charInfo)),
    _run(),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(fillLimit)
{
}

// Routine Description:
// - This is an iterator over a range of text only. No color data will be modified as the text is inserted.
// Arguments:
// - utf16Text - UTF-16 text range
OutputCellIterator::OutputCellIterator(const std::wstring_view utf16Text) :
    _mode(Mode::LooseTextOnly),
    _currentView(s_GenerateView(utf16Text)),
    _run(utf16Text),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(0)
{
}

// Routine Description:
// - This is an iterator over a range text that will apply the same color to every position.
// Arguments:
// - utf16Text - UTF-16 text range
// - attribute - Color to apply over the entire range
OutputCellIterator::OutputCellIterator(const std::wstring_view utf16Text, const TextAttribute attribute) :
    _mode(Mode::Loose),
    _currentView(s_GenerateView(utf16Text, attribute)),
    _run(utf16Text),
    _attr(attribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}

// Routine Description:
// - This is an iterator over legacy colors only. The text is not modified.
// Arguments:
// - legacyAttrs - One legacy color item per cell
// - unused - useless bool to change function signature for legacyAttrs constructor because the C++ compiler in
//             razzle cannot distinguish between a std::wstring_view and a std::basic_string_view<WORD>
// NOTE: This one internally casts to wchar_t because Razzle sees WORD and wchar_t as the same type
//       despite that Visual Studio build can tell the difference.
#pragma warning(push)
#pragma warning(suppress : 26490)
// Suppresses reinterpret_cast. We're only doing this because Windows doesn't understand the type difference between wchar_t and DWORD.
// It is not worth trying to separate that out further or risking performance over this particular warning here.
// TODO GH 2673 - Investigate real wchar_t flag in Windows and resolve this audit issue
OutputCellIterator::OutputCellIterator(const std::basic_string_view<WORD> legacyAttrs, const bool /*unused*/) noexcept :
    _mode(Mode::LegacyAttr),
    _currentView(s_GenerateViewLegacyAttr(legacyAttrs.at(0))),
    _run(std::wstring_view(reinterpret_cast<const wchar_t*>(legacyAttrs.data()), legacyAttrs.size())),
    _attr(InvalidTextAttribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}
#pragma warning(pop)

// Routine Description:
// - This is an iterator over legacy cell data. We will use the unicode text and the legacy color attribute.
// Arguments:
// - charInfos - Multiple cell with unicode text and legacy color data.
OutputCellIterator::OutputCellIterator(const std::basic_string_view<CHAR_INFO> charInfos) noexcept :
    _mode(Mode::CharInfo),
    _currentView(s_GenerateView(charInfos.at(0))),
    _run(charInfos),
    _attr(InvalidTextAttribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}

// Routine Description:
// - This is an iterator over existing OutputCells with full text and color data.
// Arguments:
// - cells - Multiple cells in a run
OutputCellIterator::OutputCellIterator(const std::basic_string_view<OutputCell> cells) :
    _mode(Mode::Cell),
    _currentView(s_GenerateView(cells.at(0))),
    _run(cells),
    _attr(InvalidTextAttribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}

// Routine Description:
// - Specifies whether this iterator is valid for dereferencing (still valid underlying data)
// Return Value:
// - True if the views on dereference are valid. False if it shouldn't be dereferenced.
OutputCellIterator::operator bool() const noexcept
{
    try
    {
        switch (_mode)
        {
        case Mode::Loose:
        case Mode::LooseTextOnly:
        {
            // In lieu of using start and end, this custom iterator type simply becomes bool false
            // when we run out of items to iterate over.
            return _pos < std::get<std::wstring_view>(_run).length();
        }
        case Mode::Fill:
        {
            if (_fillLimit > 0)
            {
                return _pos < _fillLimit;
            }
            return true;
        }
        case Mode::Cell:
        {
            return _pos < std::get<std::basic_string_view<OutputCell>>(_run).length();
        }
        case Mode::CharInfo:
        {
            return _pos < std::get<std::basic_string_view<CHAR_INFO>>(_run).length();
        }
        case Mode::LegacyAttr:
        {
            return _pos < std::get<std::wstring_view>(_run).length();
        }
        default:
            FAIL_FAST_HR(E_NOTIMPL);
        }
    }
    CATCH_FAIL_FAST();
}

// Routine Description:
// - Advances the iterator one position over the underlying data source.
// Return Value:
// - Reference to self after advancement.
OutputCellIterator& OutputCellIterator::operator++()
{
    // Keep track of total distance moved (cells filled)
    _distance++;

    switch (_mode)
    {
    case Mode::Loose:
    {
        if (!_TryMoveTrailing())
        {
            // When walking through a text sequence, we need to move forward by the number of wchar_ts consumed in the previous view
            // in case we had a surrogate pair (or wider complex sequence) in the previous view.
            _pos += _currentView.Chars().size();
            if (operator bool())
            {
                _currentView = s_GenerateView(std::get<std::wstring_view>(_run).substr(_pos), _attr);
            }
        }
        break;
    }
    case Mode::LooseTextOnly:
    {
        if (!_TryMoveTrailing())
        {
            // When walking through a text sequence, we need to move forward by the number of wchar_ts consumed in the previous view
            // in case we had a surrogate pair (or wider complex sequence) in the previous view.
            _pos += _currentView.Chars().size();
            if (operator bool())
            {
                _currentView = s_GenerateView(std::get<std::wstring_view>(_run).substr(_pos));
            }
        }
        break;
    }
    case Mode::Fill:
    {
        if (!_TryMoveTrailing())
        {
            if (_currentView.DbcsAttr().IsTrailing())
            {
                auto dbcsAttr = _currentView.DbcsAttr();
                dbcsAttr.SetLeading();

                _currentView = OutputCellView(_currentView.Chars(),
                                              dbcsAttr,
                                              _currentView.TextAttr(),
                                              _currentView.TextAttrBehavior());
            }

            if (_fillLimit > 0)
            {
                // We walk forward by one because we fill with the same cell over and over no matter what
                _pos++;
            }
        }
        break;
    }
    case Mode::Cell:
    {
        // Walk forward by one because cells are assumed to be in the form they needed to be
        _pos++;
        if (operator bool())
        {
            _currentView = s_GenerateView(std::get<std::basic_string_view<OutputCell>>(_run).at(_pos));
        }
        break;
    }
    case Mode::CharInfo:
    {
        // Walk forward by one because charinfos are just the legacy version of cells and prealigned to columns
        _pos++;
        if (operator bool())
        {
            _currentView = s_GenerateView(std::get<std::basic_string_view<CHAR_INFO>>(_run).at(_pos));
        }
        break;
    }
    case Mode::LegacyAttr:
    {
        // Walk forward by one because color attributes apply cell by cell (no complex text information)
        _pos++;
        if (operator bool())
        {
            _currentView = s_GenerateViewLegacyAttr(std::get<std::wstring_view>(_run).at(_pos));
        }
        break;
    }
    default:
        FAIL_FAST_HR(E_NOTIMPL);
    }

    return (*this);
}

// Routine Description:
// - Advances the iterator one position over the underlying data source.
// Return Value:
// - Reference to self after advancement.
OutputCellIterator OutputCellIterator::operator++(int)
{
    auto temp(*this);
    operator++();
    return temp;
}

// Routine Description:
// - Reference the view to fully-formed output cell data representing the underlying data source.
// Return Value:
// - Reference to the view
const OutputCellView& OutputCellIterator::operator*() const noexcept
{
    return _currentView;
}

// Routine Description:
// - Get pointer to the view to fully-formed output cell data representing the underlying data source.
// Return Value:
// - Pointer to the view
const OutputCellView* OutputCellIterator::operator->() const noexcept
{
    return &_currentView;
}

// Routine Description:
// - Checks the current view. If it is a leading half, it updates the current
//   view to the trailing half of the same glyph.
// - This helps us to draw glyphs that are two columns wide by "doubling"
//   the view that is returned so it will consume two cells.
// Return Value:
// - True if we just turned a lead half into a trailing half (and caller doesn't
//   need to further update the view).
// - False if this wasn't applicable and the caller should update the view.
bool OutputCellIterator::_TryMoveTrailing() noexcept
{
    if (_currentView.DbcsAttr().IsLeading())
    {
        auto dbcsAttr = _currentView.DbcsAttr();
        dbcsAttr.SetTrailing();

        _currentView = OutputCellView(_currentView.Chars(),
                                      dbcsAttr,
                                      _currentView.TextAttr(),
                                      _currentView.TextAttrBehavior());
        return true;
    }
    else
    {
        return false;
    }
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and specify that the attributes shouldn't be changed.
// Arguments:
// - view - View representing characters corresponding to a single glyph
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const std::wstring_view view)
{
    return s_GenerateView(view, InvalidTextAttribute, TextAttributeBehavior::Current);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - view - View representing characters corresponding to a single glyph
// - attr - Color attributes to apply to the text
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const std::wstring_view view,
                                                  const TextAttribute attr)
{
    return s_GenerateView(view, attr, TextAttributeBehavior::Stored);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - view - View representing characters corresponding to a single glyph
// - attr - Color attributes to apply to the text
// - behavior - Behavior of the given text attribute (used when writing)
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const std::wstring_view view,
                                                  const TextAttribute attr,
                                                  const TextAttributeBehavior behavior)
{
    const auto glyph = Utf16Parser::ParseNext(view);
    DbcsAttribute dbcsAttr;
    if (IsGlyphFullWidth(glyph))
    {
        dbcsAttr.SetLeading();
    }

    return OutputCellView(glyph, dbcsAttr, attr, behavior);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - wch - View representing a single UTF-16 character (that can be represented without surrogates)
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const wchar_t& wch) noexcept
{
    const auto glyph = std::wstring_view(&wch, 1);

    DbcsAttribute dbcsAttr;
    if (IsGlyphFullWidth(wch))
    {
        dbcsAttr.SetLeading();
    }

    return OutputCellView(glyph, dbcsAttr, InvalidTextAttribute, TextAttributeBehavior::Current);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - attr - View representing a single color
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const TextAttribute& attr) noexcept
{
    return OutputCellView({}, {}, attr, TextAttributeBehavior::StoredOnly);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - wch - View representing a single UTF-16 character (that can be represented without surrogates)
// - attr - View representing a single color
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const wchar_t& wch, const TextAttribute& attr) noexcept
{
    const auto glyph = std::wstring_view(&wch, 1);

    DbcsAttribute dbcsAttr;
    if (IsGlyphFullWidth(wch))
    {
        dbcsAttr.SetLeading();
    }

    return OutputCellView(glyph, dbcsAttr, attr, TextAttributeBehavior::Stored);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - legacyAttr - View representing a single legacy color
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateViewLegacyAttr(const WORD& legacyAttr) noexcept
{
    WORD cleanAttr = legacyAttr;
    WI_ClearAllFlags(cleanAttr, COMMON_LVB_SBCSDBCS); // don't use legacy lead/trailing byte flags for colors

    const TextAttribute attr(cleanAttr);
    return s_GenerateView(attr);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - charInfo - character and attribute pair representing a single cell
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const CHAR_INFO& charInfo) noexcept
{
    const auto glyph = std::wstring_view(&charInfo.Char.UnicodeChar, 1);

    DbcsAttribute dbcsAttr;
    if (WI_IsFlagSet(charInfo.Attributes, COMMON_LVB_LEADING_BYTE))
    {
        dbcsAttr.SetLeading();
    }
    else if (WI_IsFlagSet(charInfo.Attributes, COMMON_LVB_TRAILING_BYTE))
    {
        dbcsAttr.SetTrailing();
    }

    TextAttribute textAttr;
    textAttr.SetFromLegacy(charInfo.Attributes);

    const auto behavior = TextAttributeBehavior::Stored;
    return OutputCellView(glyph, dbcsAttr, textAttr, behavior);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// Arguments:
// - cell - A reference to the cell for which we will make the read-only view
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const OutputCell& cell)
{
    return OutputCellView(cell.Chars(), cell.DbcsAttr(), cell.TextAttr(), cell.TextAttrBehavior());
}

// Routine Description:
// - Gets the distance between two iterators relative to the input data given in.
// Return Value:
// - The number of items of the input run consumed between these two iterators.
ptrdiff_t OutputCellIterator::GetInputDistance(OutputCellIterator other) const noexcept
{
    return _pos - other._pos;
}

// Routine Description:
// - Gets the distance between two iterators relative to the number of cells inserted.
// Return Value:
// - The number of cells in the backing buffer filled between these two iterators.
ptrdiff_t OutputCellIterator::GetCellDistance(OutputCellIterator other) const noexcept
{
    return _distance - other._distance;
}
